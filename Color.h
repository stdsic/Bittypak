#ifndef __COLOR_H_
#define __COLOR_H_

#include <windows.h>
/*
	Hue(색상)
	Saturation(채도)
	Value(명도)

	3가지로 나뉘는 HSV 표현 방식은 색상을 좌표로 표현한다.
	360도 원형의 공간으로 이루어지며 주기 함수를 이용하여
	색상을 반복하거나 변경할 수 있다.

	HSV 모델에선 색상값을 각도로 표현하고,
	채도는 반지름, 명도는 높이로 표현한다.

	여기서 말하는 색상과 채도, 명도를 간단히 설명하면 다음과 같다.

	빨/주/노/초 등의 대분류를 색상이라 하며 이를 각도로 지정한다.
	채도는 색상의 깊이 곧, 정밀도 또는 뭉쳐있는 정도를 말한다.
	더 쉽게 말하면 색이 얼마나 많고 진한가를 나타낸다.
	이는 반지름으로 지정한다.

	마지막으로 명도는 색의 선명함 정도를 말하며 높이값으로 지정한다.

	일반적으로 알파값을 포함하지만, 현재 프로젝트는
	Timer 또는 메세지 루프를 이용하여 Tick단위의 애니메이션
	곧, 영상 처리를 하고 있진 않으므로 알파값을 추가하지 않았다.

	블렌딩이건 뭐건 결국 가중치를 부여하는 것이므로 영상이 아닌 이상 크게 의미가 없다.
*/

/*
	색상 자체는 COLORREF와 float 타입의 RGB를 기본 형태로 정하고 유지/관리하기로 정한다.
	메모리를 최소한으로 유지하고 싶다면 공용체를 활용해도 좋다.
*/

class Color {
public:
	BOOL _bHSV;
	float _H, _S, _V;
	float _R, _G, _B;

public:
	static const float Ratio;
	static const Color White;
	static const Color Black;
	static const Color Gray;
	static const Color WhiteGray;
	static const Color LightGray;
	static const Color Red;
	static const Color Green;
	static const Color Blue;
	static const Color Yellow;
	static const Color Cyan;
	static const Color Magenta;

public:
	/* TODO : 연산자 함수 추가 필요 */
	const Color operator +(const Color& Other) const {
		return Color(
				_R + Other._R,
				_G + Other._G,
				_B + Other._B
		);
	}

	const Color operator -(const Color& Other) const {
		return Color(
				_R - Other._R,
				_G - Other._G,
				_B - Other._B
		);
	}

	const Color operator *(const Color& Other) const {
		return Color(
				_R * Other._R,
				_G * Other._G,
				_B * Other._B
		);
	}

	const Color operator /(const Color& Other) const {
		return Color(
				_R / Other._R,
				_G / Other._G,
				_B / Other._B
		);
	}

	const Color operator +(const float& Value) const {
		return Color(
				_R + Value,
				_G + Value,
				_B + Value
		);
	}

	const Color operator -(const float& Value) const {
			return Color(
				_R - Value,
				_G - Value,
				_B - Value
		);
	}

	const Color operator *(const float& Value) const {
		return Color(
				_R * Value,
				_G * Value,
				_B * Value
		);
	}

	const Color operator /(const float& Value) const {
		return Color(
				_R / Value,
				_G / Value,
				_B / Value
		);
	}

	/*
		.cpp

	const Color operator +(const float& Value, const Color& C);
	const Color operator -(const float& Value, const Color& C);
	const Color operator *(const float& Value, const Color& C);
	const Color operator /(const float& Value, const Color& C);
	*/

public:
	float MaxColor();
	// float MaxColor() const { return max(_R, max(_G, _B)); }

public:
	COLORREF ToColorRef();
	Color ToColor();

public:
	Color(float R = 0.f, float G = 0.f, float B = 0.f, BOOL bHSV = FALSE);
	operator int() { return (int)ToColorRef(); }
	explicit Color(COLORREF ColorRef);
	~Color();
};

#endif
