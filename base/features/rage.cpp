#include "rage.h"

#include "rage/antiaim.h"
#include "../core/variables.h"
#include "../core/interfaces.h"
#include "../core/convar.h"
#include "../sdk/interfaces/iglobalvars.h"
#include "../sdk/interfaces/icliententitylist.h"
#include "../sdk/interfaces/iweaponsystem.h"
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
#pragma endregion

#pragma region rage_main
static float GetFov(const QAngle_t& angView, const Vector_t& vecStart, const Vector_t& vecEnd)
{
    QAngle_t angTo = (vecEnd - vecStart).ToAngles();
    angTo.Normalize();

    QAngle_t angDelta = angTo - angView;
    angDelta.Normalize();
    return angDelta.Length2D();
}

void RAGE::AimBot(CCSPlayer* pLocal, CUserCmd* pCmd, bool* pbSendPacket /* NEW */)
{
    if (!C::Get<bool>(Vars.bRage) || !pLocal->IsAlive())
        return;

    CBaseCombatWeapon* pWeapon = pLocal->GetActiveWeapon();

    const Vector_t vecEyePos = pLocal->GetEyePosition();
    const QAngle_t angView = pCmd->angViewPoint;

    float     flBestFov = MAX_FOV;
    QAngle_t  angBest{ };

    for (int i = 1; i <= I::Globals->nMaxClients; ++i)
    {
        CCSPlayer* pEnemy = I::ClientEntityList->Get<CCSPlayer>(i);
        if (!pEnemy || pEnemy == pLocal)
            continue;
        if (!pEnemy->IsAlive() || pEnemy->IsDormant() || !pLocal->IsOtherEnemy(pEnemy))
            continue;

        const Vector_t vecTarget = pEnemy->GetHitboxPosition(HITBOX_HEAD);
        const float    flFov = GetFov(angView, vecEyePos, vecTarget);

        if (flFov < flBestFov)
        {
            flBestFov = flFov;
            angBest = (vecTarget - vecEyePos).ToAngles();
            angBest.Normalize();
        }
    }

    if (flBestFov < MAX_FOV)
    {
        /*
         * Exactly like the sample ragebot:
         *   – store the aim angle in the command’s viewangles
         *   – No recoil is still handled afterwards in NoRecoil()
         */
        pCmd->angViewPoint = angBest;  

        if (CanFire(pLocal, pWeapon, false))
        {
            pCmd->nButtons |= IN_ATTACK;
        }

        /*
         * Ensure the final shot packet is *sent* with our new angles so the
         * server registers them. We only force this when we are actually
         * attacking; otherwise we leave the caller’s send‑packet logic untouched.
         */
        if ((pCmd->nButtons & IN_ATTACK) && pbSendPacket) 
            *pbSendPacket = true;
    }
}
#pragma endregion

void RAGE::NoRecoil(CCSPlayer* pLocal, CUserCmd* pCmd)
{
    if (C::Get<bool>(Vars.bLegit) || !C::Get<bool>(Vars.bRage))
        return;

    pCmd->angViewPoint -= pLocal->GetLocalData()->GetAimPunch() * CONVAR::weapon_recoil_scale->GetFloat();
}

#pragma region rage_callbacks
void RAGE::OnMove(CCSPlayer* pLocal, CUserCmd* pCmd, bool* pbSendPacket)
{
    ANTIAIM::OnMove(pLocal, pCmd, pbSendPacket);

    AimBot(pLocal, pCmd, pbSendPacket); // NEW – forward send‑packet pointer

    NoRecoil(pLocal, pCmd);
}
#pragma endregion
