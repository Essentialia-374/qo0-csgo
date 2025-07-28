#include "rage.h"
#include "../sdk.h"

#include "rage/antiaim.h"
#include "../core/variables.h"
#include "../core/interfaces.h"
#include "../core/convar.h"
#include "../sdk/interfaces/iglobalvars.h"
#include "../sdk/interfaces/icliententitylist.h"
#include "../sdk/interfaces/iweaponsystem.h"
#include "../features/autowall.h"
#include "../features.h"

using namespace F;

#pragma region rage_helpers
bool RAGE::CanFire(CCSPlayer* pPlayer, CBaseCombatWeapon* pWeapon, bool bCheckRevolver)
{
    if (pPlayer == nullptr || pWeapon == nullptr)
        return false;

    // ensure player can attack
    const float flServerTime = TICKS_TO_TIME(pPlayer->GetTickBase());

    if (!pPlayer->CanAttack(flServerTime))
        return false;

    // check weapon state
    if (flServerTime < pWeapon->GetNextPrimaryAttack())
        return false;

    if (bCheckRevolver)
    {
        const ItemDefinitionIndex_t nIndex = pWeapon->GetEconItemView()->GetItemDefinitionIndex();
        if (nIndex == WEAPON_REVOLVER && static_cast<CWeaponCSBase*>(pWeapon)->GetPostponeFireReadyTime() > flServerTime)
            return false;
    }

    return true;
}

void RAGE::NoRecoil(CCSPlayer* pLocal, CUserCmd* pCmd)
{
    if (C::Get<bool>(Vars.bLegit) || !C::Get<bool>(Vars.bRage))
        return;

    pCmd->angViewPoint -= pLocal->GetLocalData()->GetAimPunch() * CONVAR::weapon_recoil_scale->GetFloat();
}

float RAGE::Hitchance(CCSPlayer* pLocal, CBaseCombatWeapon* pWeapon, const QAngle_t& angShoot, CCSPlayer* pTarget, RageHitbox_t nTargetHitbox)
{
    if (!pLocal || !pWeapon || !pTarget || !pTarget->IsAlive())
        return 0.0f;

    const CCSWeaponData* pData = I::WeaponSystem->GetWeaponData(
        pWeapon->GetEconItemView()->GetItemDefinitionIndex());

    if (pData == nullptr)
        return 0.0f;

    /* pre‑compute orthonormal basis of the shot direction so that
       we can apply per‑seed spread offsets very cheaply            */
    Vector_t vForward, vRight, vUp;
    M::AngleVectors(angShoot, &vForward, &vRight, &vUp);

    const Vector_t vEyePos = pLocal->GetEyePosition();
    const Vector_t vHitBoxCentre = pTarget->GetHitboxPosition(nTargetHitbox);
    const float flRange = pData->flRange;

    int nHits = 0;

    for (int iSeed = 0; iSeed < MAX_SEEDS; ++iSeed)
    {
        /*  The game’s spread is deterministic given a seed — we replicate
            the same routine that CBaseCombatWeapon uses so that client
            prediction == server side.                                    */
        SDK::RandomSeed(iSeed + 1);

        const auto pWeaponCS = static_cast<CWeaponCSBase*>(pWeapon);
        const float flInaccuracy = pWeaponCS->GetInaccuracy();
        const float flSpread = pWeaponCS->GetSpread();

        /*  Generate two uniform random angles and radii on the unit disc.
            This is exactly what the SDK does in CBaseCombatWeapon::GetSpread() */
        const float aInacc = SDK::RandomFloat(0.f, 2.f * M::_PI);
        const float rInacc = SDK::RandomFloat(0.f, 1.f);
        const float aSpread = SDK::RandomFloat(0.f, 2.f * M::_PI);
        const float rSpread = SDK::RandomFloat(0.f, 1.f);

        //  Final per‑axis deviation
        const float x = std::cos(aInacc) * (rInacc * flInaccuracy) +
            std::cos(aSpread) * (rSpread * flSpread);

        const float y = std::sin(aInacc) * (rInacc * flInaccuracy) +
            std::sin(aSpread) * (rSpread * flSpread);

        //  Build the shot’s *real* forward vector with spread applied
        Vector_t vDir = (vForward + vRight * x + vUp * y).Normalized();

        //  End of the bullet’s path (no penetration yet)
        const Vector_t vEnd = vEyePos + vDir * flRange;

        //  We first trace *world* to get an enter surface
        Trace_t tr;
        {
            Ray_t ray(vEyePos, vEnd);
            CTraceFilterSimple filter(pLocal);        // ignore local player
            I::EngineTrace->TraceRay(ray, MASK_SHOT_HULL | CONTENTS_HITBOX,
                &filter, &tr);
        }

        // Extend the trace to players (same helper the game uses)
        SDK::ClipTraceToPlayers(vEyePos, vEnd + vDir * 40.0f,
            MASK_SHOT_HULL | CONTENTS_HITBOX,
            nullptr, &tr);

        // Fast reject – didn’t even touch requested entity
        if (tr.pHitEntity != pTarget)
            continue;

        /*  If the ray reached the target but through solid matter
            we need to verify penetration.  The autowall subsystem does
            the heavy lifting for us.                                   */
        if (tr.bStartSolid || tr.flFraction < 1.f)     // potential wall‑bang
        {
            SimulateBulletObject_t sim;
            sim.vecPosition = vEyePos;
            sim.vecDirection = vDir;

            if (!AUTOWALL::SimulateFireBullet(pLocal, pWeapon, sim) ||
                sim.enterTrace.pHitEntity != pTarget)
                continue; // either we stopped in the wall or hit other entity
        }

        // Arrived on the desired player/hit‑box   
        ++nHits;
    }

    return static_cast<float>(nHits) / static_cast<float>(MAX_SEEDS);
}

#pragma endregion

#pragma region rage_main

void RAGE::AimBot(CCSPlayer* pLocal, CUserCmd* pCmd, bool* pbSendPacket)
{
    if (!C::Get<bool>(Vars.bRage) || !pLocal->IsAlive())
        return;

    CBaseCombatWeapon* pWeapon = pLocal->GetActiveWeapon();
    if (!pWeapon)
        return;

    // Local state – eye‑pos and viewangles at the start of the tick 
    const Vector_t vecEyePos = pLocal->GetEyePosition();
    const QAngle_t angView = pCmd->angViewPoint;

    //  Best‑target bookkeeping 
    float kHitChanceThreshold = (C::Get<float>(Vars.bRageHitchance)) * 0.01f;
    float flBestFov = MAX_FOV;
    QAngle_t  angBest{ };
    CCSPlayer* pBestEnemy = nullptr;

    // Target‑selection loop 
    for (int i = 1; i <= I::Globals->nMaxClients; ++i)
    {
        CCSPlayer* pEnemy = I::ClientEntityList->Get<CCSPlayer>(i);
        if (!pEnemy || pEnemy == pLocal)
            continue;
        if (!pEnemy->IsAlive() || pEnemy->IsDormant() || !pLocal->IsOtherEnemy(pEnemy))
            continue;

        const Vector_t vecTarget = pEnemy->GetHitboxPosition(static_cast<ERageHitbox>(C::Get<int>(Vars.iRageHitbox)));
        const float flFov = M::GetFov(angView, vecEyePos, vecTarget);

        if (flFov < flBestFov)
        {
            flBestFov = flFov;
            pBestEnemy = pEnemy;

            angBest = (vecTarget - vecEyePos).ToAngles();
            angBest.Normalize();
        }
    }

    // If nothing viable was found, abort 
    if (!pBestEnemy || flBestFov >= MAX_FOV)
        return;

    // Write desired aim angle into the command
    pCmd->angViewPoint = angBest;

    //  Only attempt to shoot if weapon can actually fire this tick 
    if (CanFire(pLocal, pWeapon, /*bCheckRevolver=*/false))
    {
        /* ----------------------------------------------------------------
         *  Call Hitchance():
         *     • returns [0‒1] probability of a hit with current spread
         *     • we demand at least 70 % before pressing IN_ATTACK
         * ---------------------------------------------------------------- */
        const float flHitProb = Hitchance(pLocal, pWeapon, angBest, pBestEnemy, static_cast<RageHitbox_t>(C::Get<int>(Vars.iRageHitbox)));

        if (flHitProb >= kHitChanceThreshold)
            pCmd->nButtons |= IN_ATTACK;
    }

    // Force the shot packet to be sent when we did decide to shoot
    if ((pCmd->nButtons & IN_ATTACK) && pbSendPacket)
        *pbSendPacket = true;
}
#pragma endregion

#pragma region rage_callbacks
void RAGE::OnMove(CCSPlayer* pLocal, CUserCmd* pCmd, bool* pbSendPacket)
{
    /*
     * 1) Pick a target, write the final shot angle and set IN_ATTACK
     *    (may also force the packet by setting *pbSendPacket = true).
     */
    AimBot(pLocal, pCmd, pbSendPacket);

    /*
     * 2) Remove weapon recoil from *that* real shot angle.
     */
    NoRecoil(pLocal, pCmd);

    /*
     * 3) Apply fake / real anti‑aim logic **after** we know whether we
     *    are shooting.  If IN_ATTACK is set, AntiAim::OnMove() returns
     *    instantly and leaves the aimbot’s angle untouched.
     */
    ANTIAIM::OnMove(pLocal, pCmd, pbSendPacket);
}

#pragma endregion
