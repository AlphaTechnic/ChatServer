#pragma once
#include "pch.h"

// 스레드 안전한 콘솔 출력
void Log(const std::string& message);

// 범위 내 정수 난수 생성
int GetRandomInt(int min, int max);

// 범위 내 실수 난수 생성 (확률용)
double GetRandomDouble(double min, double max);
