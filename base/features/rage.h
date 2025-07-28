#pragma once
#include "../common.h"

#include "../sdk/datatypes/usercmd.h"

// used: ccsplayer
#include "../sdk/entity.h"

/*
 * RAGE
 * - strong assistance to the user against other cheaters
 */
namespace F::RAGE
{
    /* @section: callbacks */
    void OnMove(CCSPlayer* pLocal, CUserCmd* pCmd, bool* pbSendPacket);

    bool CanFire(CCSPlayer* pPlayer, CBaseCombatWeapon* pWeapon, bool bCheckRevolver);

    /* @section: main */
    void AimBot(CCSPlayer* pLocal, CUserCmd* pCmd, bool* pbSendPacket);

    void NoRecoil(CCSPlayer* pLocal, CUserCmd* pCmd);
}
