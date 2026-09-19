#pragma once
#include "Math/Vector/Vector3.hpp"
#include <string>
#ifndef BOOL3_H
#define BOOL3_H

enum class Bool3 {
    True,
    False,
    Unknown
};

enum class XPose {
    Standing = 0,   // Standing (idle or walking normally)
    Crouching = 1,  // Crouched
    Dashing = 2,    // Running / Sprinting
    Creeping = 3,   // Prone (lying on the ground)
    Jumping = 11,   // Jumping (in air)
    Knocked = 8     // Knocked down (injured/incapacitated)
};


class Player {
public:
    bool IsBot;
    bool IsKnown;
    bool IsDead;
    bool IsKnocked;
    bool IsVisible;
    XPose Pose;
    float Distance;
    short Health;
    short Gun;
    short Ammo;  // Current ammo count
    short MaxAmmo;  // Maximum ammo capacity
    Bool3 IsTeam;

    std::string Name;
    std::string RankText;
    uint32_t Address;
    Vector3 Head;       // Cabe�a
    Vector3 Neck;       // Pesco�o
    Vector3 RightShoulder; // Ombro direito
    Vector3 LeftShoulder;  // Ombro esquerdo
    Vector3 RightElbow;    // Cotovelo direito
    Vector3 LeftElbow;     // Cotovelo esquerdo
    Vector3 RightWrist;    // Pulso direito
    Vector3 LeftWrist;     // Pulso esquerdo
    Vector3 LeftHand;      // M�o esquerda
    Vector3 RightHand;     // M�o direita

    Vector3 Hip;          // Quadril
    Vector3 Groin;        // Virilha

    Vector3 RightAnkle;   // Tornozelo direito
    Vector3 LeftAnkle;    // Tornozelo esquerdo
    Vector3 LeftFoot;     // P� esquerdo
    Vector3 RightFoot;    // P� direito

    Vector3 Root;         // Raiz do corpo
    Vector3 RootBone;     // Raiz adicional do corpo
    Vector3 LastHead;
    Vector3 Velocity; // estimated per tick
};

#endif
