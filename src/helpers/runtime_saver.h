#pragma once
#include "../valve_sdk/math/Vectors.hpp"

struct ImpactInfoStruct
{
	Vector ShotPos = Vector(0, 0, 0);
	Vector Pos = Vector(0, 0, 0);
	float Time = 0.f;
	int Tick = 0;
};

struct BulletTracerInfoStruct
{
	bool DidHit = false;
	float Time = 0.f;
	Vector ShotPos = Vector(0, 0, 0);
	Vector HitPos = Vector(0, 0, 0);
};

struct HitmarkerInfoStruct
{
	float HitTime = 0.f;
	float Damage = 0.f;
};

class runtime_saver
{
public:
	std::vector<ImpactInfoStruct> hitpositions;
	std::vector<BulletTracerInfoStruct> BulletTracers;
	HitmarkerInfoStruct HitmarkerInfo;

	Vector LastShotEyePos = Vector(0, 0, 0);
};

extern runtime_saver saver;
