#pragma once
#include "../common.h"

#include "../sdk/datatypes/usercmd.h"

// used: ccsplayer
#include "../sdk/entity.h"

#include "../utilities/math.h"

#define MAX_SEEDS 128

/*
 * RAGE
 * - strong assistance to the user against other cheaters
 */
namespace F::RAGE
{
    using RageHitbox_t = int;

    enum ERageHitbox : RageHitbox_t
    {
        RAGE_HITBOX_HEAD = HITBOX_HEAD,
        RAGE_HITBOX_NECK = HITBOX_NECK,
        RAGE_HITBOX_CHEST = HITBOX_CHEST,
        RAGE_HITBOX_STOMACH = HITBOX_STOMACH,
        RAGE_HITBOX_PELVIS = HITBOX_PELVIS,
        RAGE_HITBOX_MAX = HITBOX_MAX
    };

    /* @section: callbacks */
    void OnMove(CCSPlayer* pLocal, CUserCmd* pCmd, bool* pbSendPacket);

    bool CanFire(CCSPlayer* pPlayer, CBaseCombatWeapon* pWeapon, bool bCheckRevolver);

    /* @section: main */
    void AimBot(CCSPlayer* pLocal, CUserCmd* pCmd, bool* pbSendPacket);

   /*  Build an array of world–space points that will be scanned on a given
    *  enemy hit‑box.  The function always returns at least the box centre and
    *  then – depending on `flPointScale` – additional cardinal / diagonal
    *  points that sit on the surface of the hit‑box capsule.
    *
    *  @param pTarget        – player we are generating points for
    *  @param iHitbox        – CS / Rage hit‑box id (HITBOX_HEAD …)
    *  @param flPointScale   – 0‒1, how far from the centre extra points are
    *  @return std::vector   – ready‑to‑trace points in world space
    */
    std::vector<Vector_t> MultiPoints(CCSPlayer* pTarget, RageHitbox_t iHitbox, float flPointScale = 0.75f);

    void NoRecoil(CCSPlayer* pLocal, CUserCmd* pCmd);

    float Hitchance(CCSPlayer* pLocal, CBaseCombatWeapon* pWeapon, const QAngle_t& angShoot, CCSPlayer* pTarget, int iTargetHitbox);

    void UpdateHitboxes(std::vector<RageHitbox_t>& hitboxes, CBaseCombatWeapon* pWeapon);
}
