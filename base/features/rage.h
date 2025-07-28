#pragma once
#include "../common.h"

#include "../sdk/datatypes/usercmd.h"

// used: ccsplayer
#include "../sdk/entity.h"

#include "../utilities/math.h"
#include "../sdk/interfaces/ivmodelinfo.h"

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

    void NoRecoil(CCSPlayer* pLocal, CUserCmd* pCmd);

    float Hitchance(CCSPlayer* pLocal, CBaseCombatWeapon* pWeapon, const QAngle_t& angShoot, CCSPlayer* pTarget, RageHitbox_t nTargetHitbox);
}
