//********************************************************* 
// 
// Copyright (c) Microsoft. All rights reserved. 
// This code is licensed under the MIT License (MIT). 
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF 
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY 
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR 
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT. 
// 
//*********************************************************

#include "pch.h"

#include "winioctl.h"
#include "ntddvdeo.h"

//#include "BasicMath.h"
#include "ColorSpaces.h"
#include "Game.h"
#include "BackgroundNoiseEffect.h"
#include "BandedGradientEffect.h"
#include "SineSweepEffect.h"
#include "ToneSpikeEffect.h"

#include <winrt\Windows.Devices.Display.h>
#include <winrt\Windows.Devices.Enumeration.h>
#include <winrt\Windows.Foundation.h>

#define BRIGHTNESS_SLIDER_FACTOR (m_rawOutDesc.MaxLuminance / m_outputDesc.MaxLuminance)

#define PATCHPCT (0.08f)
#define NUMXRITECOLORS (97.f)
struct XriteColors		// provided courtesy of Portrait X-Rite(TM) calibration technology
{
	int num;
	int row;
	char col;
	UINT32 RGB;
	float  R, G, B;		// for 100 nit D65 white point in 2020 gamut primaries
	double X, Y, Z;		// (CIE 1931 2ø)
} XRite[] = {
//n row col    RRGGBB      R          G           B         X             Y             Z
//0, 0, '-', 0xFFFFFF,  1.000000, 1.000000, 1.000000, 100.000000000,100.000000000,100.000000000,
  1, 1, 'A', 0xF4F6F3,  0.498834, 0.499625, 0.497344,  86.533211290, 91.585525820, 97.878649400,
  2, 1, 'B', 0x818181,  0.364688, 0.364859, 0.365180,  20.792432080, 21.884200640, 23.925671570,
  3, 1, 'C', 0x3C3C3B,  0.240312, 0.240033, 0.239499,   4.256057219,  4.474643989,  4.833733497,  // skip 4-15
 16, 2, 'B', 0x933E6E,  0.359104, 0.268128, 0.332106,  16.526303580, 10.753371090, 16.008163310,
 17, 2, 'C', 0x55445F,  0.283570, 0.261789, 0.306806,   7.899895441,  6.898727382, 11.774254880,
 18, 2, 'D', 0xCCD6E3,  0.462816, 0.468296, 0.480466,  62.735330580, 66.451241440, 82.240083590,
 19, 2, 'E', 0x765446,  0.330253, 0.294836, 0.267084,  11.750768390, 10.590861790,  7.186192248,
 20, 2, 'F', 0xC59482,  0.431643, 0.396995, 0.371157,  37.583017980, 34.669699420, 25.836393250,
 21, 2, 'G', 0x5F7B9D,  0.333089, 0.353966, 0.399236,  17.837678130, 18.961502280, 34.681355200,
 22, 2, 'H', 0x5A6E42,  0.313280, 0.333256, 0.266624,  10.832807340, 13.750247450,  7.303778533,
 23, 2, 'I', 0x8280AF,  0.369187, 0.365089, 0.420534,  24.670403040, 23.327084940, 43.628533060,
 24, 2, 'J', 0x65BEAD,  0.383048, 0.437697, 0.423882,  31.263243540, 42.546305730, 45.948413660,
 25, 2, 'K', 0xF2CFB8,  0.483620, 0.464457, 0.440228,  67.553141900, 67.104629060, 54.824288420,
 26, 2, 'L', 0x6E3D46,  0.311767, 0.254055, 0.263454,   9.248634896,  7.123491009,  6.742289138,
 27, 2, 'M', 0xBC3F64,  0.403831, 0.284703, 0.318748,  24.886900670, 15.231543150, 13.686284150,  // skip 28, 29
 30, 3, 'B', 0xB88BB9,  0.420486, 0.384844, 0.433509,  37.638886490, 32.008638080, 50.160484400,
 31, 3, 'C', 0x7266A6,  0.341450, 0.326192, 0.408593,  18.644913870, 15.907477420, 38.201392190,
 32, 3, 'D', 0xEFCDD6,  0.482344, 0.462548, 0.468715,  69.637304040, 67.011486060, 73.044700990,
 33, 3, 'E', 0xDF7F33,  0.446294, 0.375568, 0.261945,  38.524340580, 31.114525100,  7.148722041,
 34, 3, 'F', 0x4B5EAB,  0.298593, 0.308490, 0.413195,  14.222416490, 12.384797420, 40.058298340,
 35, 3, 'G', 0xC35561,  0.413632, 0.315869, 0.317071,  27.841684530, 18.923019180, 13.524133140,
 36, 3, 'H', 0x5C3F6B,  0.291663, 0.253529, 0.325210,   8.848467026,  6.856047893, 14.710645450,
 37, 3, 'I', 0xA1BE44,  0.417937, 0.439934, 0.302363,  34.063394940, 44.746591560, 12.372120840,
 38, 3, 'J', 0xE2A22D,  0.456973, 0.415694, 0.269431,  44.613171700, 42.127022240,  8.225577179,
 39, 3, 'K', 0xC6EBD5,  0.466355, 0.486926, 0.469523,  64.899359280, 76.071553550, 74.137955910,
 40, 3, 'L', 0xCA3C3C,  0.416690, 0.285280, 0.254187,  26.901992700, 16.175839940,  6.031991297,
 41, 3, 'M', 0x603C50,  0.293062, 0.247799, 0.280432,   7.891606323,  6.263674361,  8.412485400,  // skip 42, 43
 44, 4, 'B', 0x7C3B91,  0.334272, 0.257589, 0.380366,  14.984765510,  9.453061058, 27.850117650,
 45, 4, 'C', 0x324A74,  0.247897, 0.269742, 0.338992,   6.899378575,  6.859692772, 17.368376520,
 46, 4, 'D', 0xB0E0D5,  0.448749, 0.475551, 0.468542,  56.546648740, 67.176637550, 73.170544650,
 47, 4, 'E', 0x364191,  0.251805, 0.254680, 0.380224,   8.563477857,  6.664989920, 27.799448970,
 48, 4, 'F', 0x47974B,  0.329089, 0.389127, 0.292306,  14.858863500, 23.829750370, 10.461868100,
 49, 4, 'G', 0xB3323E,  0.391908, 0.262092, 0.253351,  20.698947040, 12.250337220,  5.902634969,
 50, 4, 'H', 0xEBC821,  0.474327, 0.455687, 0.280104,  55.123904260, 58.969821910,  9.885304636,
 51, 4, 'I', 0xBF5696,  0.411869, 0.317747, 0.390308,  30.256590580, 19.922766010, 31.288718640,
 52, 4, 'J', 0x008AA9,  0.288986, 0.371841, 0.414080,  15.367801500, 20.604087770, 40.791705610,
 53, 4, 'K', 0xDBD3E0,  0.471163, 0.466250, 0.478069,  65.832282760, 66.887123200, 80.264910000,
 54, 4, 'L', 0xCF8295,  0.435661, 0.376770, 0.392828,  39.022037260, 31.281204360, 32.516169310,
 55, 4, 'M', 0xBD3749,  0.402886, 0.273609, 0.274269,  23.574967010, 14.052860450,  7.823662074,  // skip 56, 57
 58, 5, 'B', 0x1589CC,  0.309459, 0.371737, 0.451158,  20.087209920, 22.344742210, 60.110134410,
 59, 5, 'C', 0x55A2CB,  0.358090, 0.406097, 0.452458,  27.505824980, 32.203205660, 61.229380280,
 60, 5, 'D', 0xF1CECA,  0.483452, 0.463290, 0.457837,  69.059715320, 67.146317300, 65.498748310,
 62, 5, 'F', 0xC8C8C9,  0.454365, 0.454102, 0.454591,  55.059386940, 57.830193630, 63.228199920,
 63, 5, 'G', 0xA3A4A5,  0.411982, 0.412326, 0.413178,  35.241397610, 37.093821930, 40.779328730,
 65, 5, 'I', 0x626262,  0.315105, 0.315390, 0.316092,  11.556000200, 12.163244770, 13.364081910,
 66, 5, 'J', 0x434445,  0.257156, 0.258030, 0.260185,   5.464348778,  5.756852478,  6.462477420,
 67, 5, 'K', 0xB4DCE2,  0.450069, 0.472251, 0.479783,  58.061821080, 66.239982950, 81.760975930,
 68, 5, 'L', 0xD58482,  0.441048, 0.379923, 0.370181,  39.789279170, 32.226929400, 25.408667110,
 69, 5, 'M', 0xE74D48,  0.445733, 0.317605, 0.280531,  36.849623790, 22.849407730,  8.634066840,  // skip 70, 71
 72, 6, 'B', 0x32A8C3,  0.344687, 0.411823, 0.444856,  25.228780790, 32.684552730, 56.694721920,
 73, 6, 'C', 0x284E5F,  0.238055, 0.276002, 0.307405,   5.692425950,  6.780516852, 11.897590810,
 74, 6, 'D', 0xCFDAA5,  0.463829, 0.471380, 0.421437,  57.590018830, 66.018199150, 45.368799830,
 76, 6, 'F', 0x59595A,  0.299838, 0.300238, 0.301483,   9.564253689, 10.064608710, 11.134841100,
 77, 6, 'G', 0x6A6B6B,  0.330352, 0.330736, 0.331527,  13.923263050, 14.660007630, 16.124553600,
 78, 6, 'H', 0x989899,  0.397295, 0.397368, 0.398364,  30.004027410, 31.537344570, 34.699049160,
 79, 6, 'I', 0xBDBDBE,  0.442224, 0.442057, 0.442609,  48.525018480, 50.982961880, 55.792765100,
 80, 6, 'J', 0xE0E1E1,  0.479188, 0.479766, 0.480139,  71.247395410, 75.107046350, 82.206832430,
 81, 6, 'K', 0xB2B2B3,  0.429090, 0.429287, 0.430159,  42.317392640, 44.512028860, 48.915528100,
 82, 6, 'L', 0xF4752D,  0.462019, 0.368007, 0.253055,  44.131007750, 32.177121340,  6.344859403,
 83, 6, 'M', 0xFFBC31,  0.493019, 0.447417, 0.290890,  64.079413780, 59.655086600, 10.992190490,  // skip 84, 85
 86, 7, 'B', 0x2B4F4F,  0.240023, 0.277619, 0.279228,   5.230441806,  6.723176467,  8.352868857,
 87, 7, 'C', 0x759ECF,  0.376705, 0.402414, 0.456569,  30.780046770, 32.682966400, 63.818882210,
 88, 7, 'D', 0xD38866,  0.439892, 0.384637, 0.334056,  38.254848490, 32.547350950, 16.965111700,
 89, 7, 'E', 0xECB49B,  0.471569, 0.436637, 0.406738,  56.916514990, 52.865360010, 38.407424560,
 90, 7, 'F', 0xBE9778,  0.427175, 0.399974, 0.358764,  35.859866060, 34.637042750, 22.564393980,
 91, 7, 'G', 0x916A50,  0.368336, 0.334573, 0.291096,  18.242782700, 16.929283660,  9.935993265,
 92, 7, 'H', 0xC6A18E,  0.437239, 0.411973, 0.388115,  40.995412260, 39.472809370, 31.230776140,
 93, 7, 'I', 0xA16645,  0.383626, 0.330956, 0.272423,  20.552375850, 17.498776050,  7.850930456,
 94, 7, 'J', 0xD09078,  0.439537, 0.393557, 0.357762,  39.377642320, 34.722317270, 22.252946180,
 95, 7, 'K', 0x777777,  0.350299, 0.350419, 0.350463,  17.591371710, 18.517812990, 20.183763000,
 96, 7, 'L', 0xC1BA0C,  0.439649, 0.437897, 0.254956,  39.620944430, 46.434482250,  7.229951406,
 97, 7, 'M', 0xF4C904,  0.480913, 0.457776, 0.269437,  58.281188360, 61.077446520,  8.830551879,  // skip 98, 99
100, 8, 'B', 0x18ABAB,  0.336325, 0.414111, 0.419404,  22.226046050, 32.146142550, 43.556240210,
101, 8, 'C', 0x009690,  0.291845, 0.387396, 0.385725,  14.523757570, 23.159776430, 30.192359560,
102, 8, 'D', 0xC89684,  0.434688, 0.399257, 0.374270,  38.792469530, 35.648401980, 26.754607140,
103, 8, 'E', 0xF0A591,  0.470462, 0.421585, 0.393360,  54.585824170, 47.552870650, 33.151261110,
104, 8, 'F', 0xBF9A88,  0.429034, 0.402877, 0.379152,  37.466446930, 35.892141540, 28.254225400,
105, 8, 'G', 0xC19989,  0.430383, 0.401702, 0.379994,  37.854159750, 35.785057360, 28.501861920,
106, 8, 'H', 0xC39986,  0.432243, 0.402706, 0.376863,  38.326792330, 36.215167020, 27.557513300,
107, 8, 'I', 0x7D5A45,  0.340984, 0.306668, 0.267055,  13.240071880, 12.144334740,  7.226119588,
108, 8, 'J', 0xD29879,  0.443156, 0.402747, 0.361315,  41.209678110, 37.455523660, 23.238538180,
109, 8, 'K', 0x494A4A,  0.269673, 0.269785, 0.271010,   6.443906197,  6.770644576,  7.488444555,
110, 8, 'L', 0xB39A45,  0.417464, 0.401490, 0.291054,  31.266612070, 33.197161740, 10.425517790,
111, 8, 'M', 0xB1BA2C,  0.427692, 0.436537, 0.275347,  36.057458440, 44.517234190,  9.083313607,  // skip 112, 113
114, 9, 'B', 0x4D4841,  0.272895, 0.267784, 0.253529,   6.360790471,  6.640126351,  5.930663147,
115, 9, 'C', 0x58AB77,  0.358377, 0.414806, 0.358557,  21.892603240, 32.388512910, 22.671978430,
116, 9, 'D', 0x00966B,  0.303113, 0.387440, 0.337556,  13.423201510, 22.836472970, 17.680583050,
117, 9, 'E', 0x345044,  0.248617, 0.279043, 0.259748,   5.323550029,  6.868151643,  6.477754156,
118, 9, 'F', 0x3EAB89,  0.345338, 0.414493, 0.380557,  21.027393740, 31.892228140, 28.814915220,
119, 9, 'G', 0x79A759,  0.377192, 0.411624, 0.317874,  23.485085090, 32.329292180, 14.345078770,
120, 9, 'H', 0x39953E,  0.318414, 0.386510, 0.273172,  13.282935450, 22.690171490,  8.297622316,
121, 9, 'I', 0x49B14F,  0.352433, 0.421254, 0.306759,  19.833708900, 33.329363180, 12.744002250,
122, 9, 'J', 0xC3904F,  0.427704, 0.391774, 0.302679,  33.918413780, 32.207291480, 11.849344930,
123, 9, 'K', 0x9AA348,  0.400663, 0.409218, 0.295429,  27.607046040, 33.474874880, 11.055671170,
124, 9, 'L', 0xA0C132,  0.418941, 0.443273, 0.283704,  34.220708820, 45.934211170, 10.083474300,
125, 9, 'M', 0x5B453D,  0.288689, 0.264016, 0.246976,   7.270479024,  6.817176846,  5.415485843,
126, 0, 'Z', 0x000000,  0.000000, 0.000000, 0.000000,   0.000000000,  0.000000000,  0.000000000,	// black
127, 0, 'Z', 0xFFFFFF,  0.508057, 0.508057, 0.508057,  95.0470000,   100.00000000, 108.88300000		// white
 };

 // Keep value in range from min to max by clamping
 float clamp(float v, float min, float max)		// inclusive
 {
	 if (v > max)
		 v = max; else
		 if (v < min)
			 v = min;
	 return v;
 }

 // Keep value in from min to max by wrapping
 float wrap(float v, float min, float max)			// inclusive
 {
	 float range = max - min + 1.f;
	 while (v >= max)
		 v -= range;
	 while (v < min)
		 v += range;
	 return v;
 }
  
//using namespace concurrency;
using namespace winrt::Windows::Devices;
using namespace winrt::Windows::Devices::Display;
using namespace winrt::Windows::Devices::Enumeration;

void ACPipeline();

extern void ExitGame();

using namespace DirectX;

using Microsoft::WRL::ComPtr;

Game::Game(PWSTR appTitle)
{
    m_appTitle = appTitle;

    ConstructorInternal();
}

void Game::ConstructorInternal()
{
	m_shiftKey = false;			// whether shift key is pressed
    m_currentTest = TestPattern::StartOfTest;
	m_currentColor = 0;			// Which of R/G/B to test.
	m_currentProfileTile = 0;	// which intensity profile tile we are on
	m_maxPQCode = 1;
	m_maxProfileTile = 1;

    m_gradientColor = D2D1::ColorF(0.25f, 0.25f, 0.25f);
    m_gradientAnimationBase = 0.25f;
    m_flashOn = false;
    m_testingTier = DisplayHDR400;
    m_outputDesc = { 0 };
	m_rawOutDesc.MaxLuminance = 0.f;
	m_rawOutDesc.MaxFullFrameLuminance = 0.f;
	m_rawOutDesc.MinLuminance = 0.f;
	m_totalTime = 0;
    m_showExplanatoryText = true;
    m_gamutVolume = 0.0f;

	m_currentBlack = 0;			// Which black level is crushed

	m_checkerboard = Checkerboard::Cb6x4;
	m_snoodDiam = 40.f;							// OD of sensor snood in mm
	m_LocalDimmingBars = 0;						// v1.2 Local Dimming Contrast test
	m_subtitleVisible = 1;						// v1.4 subTitle Flicker Test
	m_currentXRiteIndex = 0;					// v1.5 current index into array of Xrite patch colors
	m_XRitePatchAutoMode = FALSE;				// v1.5 flag for when it auto animates
	m_XRitePatchDisplayTime = 1.f;				// v1.5 duration to show color patch in auto mode
//	m_XRitePatchTimer = 0;						// v1.5 timer to run until DisplayTime is up.
	m_whiteLevelBracket = 1;					// what tier/level we should use
	m_brightnessBias = 0;						// correction for panel

	//	These are sRGB code values for HDR10:
	m_maxEffectivesRGBValue = -1;	// not set yet
	m_maxFullFramesRGBValue = -1;
	m_minEffectivesRGBValue = -1;

	//	These are PQ code values for HDR10:
	m_maxEffectivePQValue = -1;	// not set yet
	m_maxFullFramePQValue = -1;
	m_minEffectivePQValue = -1;

	// enable adjustment to tests 5.x for active dimming
	m_staticContrastsRGBValue = 0.0f;
	m_staticContrastPQValue = 0.0f;
	m_activeDimming50PQValue = 0.0f;
	m_activeDimming05PQValue = 0.0f;

	m_deviceResources = std::make_unique<DX::DeviceResources>();

	m_testPatternResources[TestPattern::TenPercentPeak]    = TestPatternResources{ std::wstring(L"Background Noise")                          , std::wstring()                                , std::wstring(L"BackgroundNoiseEffect.cso")   , CLSID_CustomBackgroundNoiseEffect };
	m_testPatternResources[TestPattern::BitDepthPrecision] = TestPatternResources{ std::wstring(L"7. Bit-Depth/Precision")                    , std::wstring()                                , std::wstring(L"BandedGradientEffect.cso")    , CLSID_CustomBandedGradientEffect };
	m_testPatternResources[TestPattern::SharpeningFilter]  = TestPatternResources{ std::wstring(L"Fresnel zone plate (sharpening test)")      , std::wstring()                                , std::wstring(L"SineSweepEffect.cso")         , CLSID_CustomSineSweepEffect };
	m_testPatternResources[TestPattern::ToneMapSpike]      = TestPatternResources{ std::wstring(L"ST.2084 Spike (Tone map test)")             , std::wstring()                                , std::wstring(L"ToneSpikeEffect.cso")         , CLSID_CustomToneSpikeEffect };
	m_testPatternResources[TestPattern::OnePixelLinesBW]   = TestPatternResources{ std::wstring(L"Single pixel lines (black/white)")          , std::wstring(L"OnePixelLinesBW1200x700.png")  , std::wstring()                               , {} };
    m_testPatternResources[TestPattern::OnePixelLinesRG]   = TestPatternResources{ std::wstring(L"Single pixel lines (red/green)")            , std::wstring(L"OnePixelLinesRG1200x700.png")  , std::wstring()                               , {} };
    m_testPatternResources[TestPattern::TextQuality]       = TestPatternResources{ std::wstring(L"Antialiased text (ClearType and grayscale)"), std::wstring(L"CalibriBoth96Dpi.png")         , std::wstring()                               , {} };

    m_hideTextString = std::wstring(L"Press SPACE to hide this text.");

    m_deviceResources->RegisterDeviceNotify(this);
}

// Initialize the Direct3D resources required to run.
void Game::Initialize(HWND window, int width, int height)
{
    m_deviceResources->SetWindow(window, width, height);
    m_deviceResources->CreateDeviceResources();
    m_deviceResources->SetDpi(96.0f);     // TODO: using default 96 DPI for now
    m_deviceResources->CreateWindowSizeDependentResources();

    CreateDeviceIndependentResources();
    CreateDeviceDependentResources();
    CreateWindowSizeDependentResources();

    m_timer.SetFixedTimeStep(true);
    m_timer.SetTargetElapsedSeconds(1.0 / 60);
}

// Returns whether the reported display metadata consists of
// default values generated by Windows.
bool Game::CheckForDefaults()
{
    return (
        // Default SDR display values (RS2)
        (270.0f == m_rawOutDesc.MaxLuminance &&
         270.0f == m_rawOutDesc.MaxFullFrameLuminance &&
         0.5f == m_rawOutDesc.MinLuminance) ||
        // Default HDR display values (RS2)
        (550.0f == m_rawOutDesc.MaxLuminance &&
         450.0f == m_rawOutDesc.MaxFullFrameLuminance &&
         0.5f == m_rawOutDesc.MinLuminance) ||
        // Default HDR display values (RS3)
        (1499.0f == m_rawOutDesc.MaxLuminance &&
         799.0f == m_rawOutDesc.MaxFullFrameLuminance &&
         0.01f == m_rawOutDesc.MinLuminance)
        );
}

// Default to tier based on what the EDID specified
Game::TestingTier Game::GetTestingTier()
{
#define SAFETY (0.970)
         if (m_rawOutDesc.MaxLuminance >= SAFETY * 10000.0f)
        return TestingTier::DisplayHDR10000;
    else if (m_rawOutDesc.MaxLuminance >= SAFETY * 6000.0f)
        return TestingTier::DisplayHDR6000;
    else if (m_rawOutDesc.MaxLuminance >= SAFETY * 4000.0f)
        return TestingTier::DisplayHDR4000;
	else if (m_rawOutDesc.MaxLuminance >= SAFETY * 3000.0f)
		return TestingTier::DisplayHDR3000;
	else if (m_rawOutDesc.MaxLuminance >= SAFETY * 2000.0f)
        return TestingTier::DisplayHDR2000;
    else if (m_rawOutDesc.MaxLuminance >= SAFETY * 1400.0f)
        return TestingTier::DisplayHDR1400;
    else if (m_rawOutDesc.MaxLuminance >= SAFETY * 1000.0f)
        return TestingTier::DisplayHDR1000;
    else if (m_rawOutDesc.MaxLuminance >= SAFETY * 600.0f)
        return TestingTier::DisplayHDR600;
	else if (m_rawOutDesc.MaxLuminance >= SAFETY * 500.0f)
			 return TestingTier::DisplayHDR500;
	else
        return TestingTier::DisplayHDR400;
}

WCHAR *Game::GetTierName(Game::TestingTier tier)
{
         if (tier == DisplayHDR400)   return L"DisplayHDR400";
	else if (tier == DisplayHDR500)   return L"DisplayHDR500";
	else if (tier == DisplayHDR600)   return L"DisplayHDR600";
	else if (tier == DisplayHDR1000)  return L"DisplayHDR1000";
	else if (tier == DisplayHDR1400)  return L"DisplayHDR1400";
    else if (tier == DisplayHDR2000)  return L"DisplayHDR2000";
	else if (tier == DisplayHDR3000)  return L"DisplayHDR3000";
	else if (tier == DisplayHDR4000)  return L"DisplayHDR4000";
    else if (tier == DisplayHDR6000)  return L"DisplayHDR6000";
    else if (tier == DisplayHDR10000) return L"DisplayHDR10000";
    else return L"Unsupported DisplayHDR Tier";
}

float Game::GetTierLuminance(Game::TestingTier tier)
{
	     if (tier == DisplayHDR400)	  return  400.f;
	else if (tier == DisplayHDR500)   return  500.f;
	else if (tier == DisplayHDR600)   return  600.f;
	else if (tier == DisplayHDR1000)  return  1015.27f;
	else if (tier == DisplayHDR1400)  return  1400.f;
	else if (tier == DisplayHDR2000)  return  2000.f;
	else if (tier == DisplayHDR3000)  return  3000.f;
	else if (tier == DisplayHDR4000)  return  4000.f;
	else if (tier == DisplayHDR6000)  return  6000.f;
	else if (tier == DisplayHDR10000) return 10000.f;
	else return -1.0f;
}

void Game::InitEffectiveValues()
{
	if (m_maxEffectivePQValue < 0.0)
	{
		float val = 1023.0f * Apply2084(m_rawOutDesc.MaxLuminance / 10000.f);
		m_maxEffectivePQValue = val - 5.0f;
	}
	if (m_maxFullFramePQValue < 0.0)
	{
		float val = 1023.0f * Apply2084(m_rawOutDesc.MaxFullFrameLuminance / 10000.f);
		m_maxFullFramePQValue = val - 5.0f;
	}
	if (m_minEffectivePQValue < 0.0)
	{
		float val = 1023.0f * Apply2084(m_rawOutDesc.MinLuminance / 10000.f);
		m_minEffectivePQValue = val + 5.0f;
	}

	if (m_maxEffectivesRGBValue < 0.0)
	{
		float val = 255;
		m_maxEffectivesRGBValue = val - 5.0f;
	}
	if (m_maxFullFramesRGBValue < 0.0)
	{
		float val = 255;
		m_maxFullFramesRGBValue = val - 5.0f;
	}
	if (m_minEffectivesRGBValue < 0.0)
	{
		float val = 10;
		m_minEffectivesRGBValue = val + 5.0f;
	}
}



#pragma region Frame Update
// Executes the basic game loop.
void Game::Tick()
{
    m_timer.Tick([&]()
    {
        Update(m_timer);
    });


    Render();
}

// Update any parameters used for animations.
void Game::Update(DX::StepTimer const& timer)
{
    m_totalTime = float(timer.GetTotalSeconds());

    D2D1_COLOR_F endColor = D2D1::ColorF(D2D1::ColorF::Black, 1);

    switch (m_currentTest)
    {
	case TestPattern::PanelCharacteristics:
		UpdateDxgiColorimetryInfo();
		break;

    case TestPattern::WarmUp:
        if (m_newTestSelected)
        {
            m_testTimeRemainingSec = 60.0f*30.0f;	// 30 minutes
        }
        else
        {
            m_testTimeRemainingSec -= static_cast<float>(timer.GetElapsedSeconds());
            m_testTimeRemainingSec = std::max(0.0f, m_testTimeRemainingSec);
        }
        break;

    case TestPattern::Cooldown:
        if (m_newTestSelected)
        {
            m_testTimeRemainingSec = 60.0f;		// One minute
        }
        else
        {
            m_testTimeRemainingSec -= static_cast<float>(timer.GetElapsedSeconds());
            m_testTimeRemainingSec = std::max(0.0f, m_testTimeRemainingSec);
        }
        if (m_testTimeRemainingSec <= 0.0f)
            SetTestPattern(m_cachedTest);
        break;

    case TestPattern::FlashTest:
    case TestPattern::FlashTestMAX:
        if (m_newTestSelected)
        {
            m_testTimeRemainingSec = 4.0f; // 4 seconds
            m_flashOn = false;
        }
        else
        {
            // TODO make this show 10ths of a second.
            m_testTimeRemainingSec -= static_cast<float>(timer.GetElapsedSeconds());
            m_testTimeRemainingSec = std::max(0.0f, m_testTimeRemainingSec);
            if (m_testTimeRemainingSec <= 0.0001)
            {
                if (!m_flashOn)
                {
                    m_flashOn = true;
                    m_testTimeRemainingSec = 2;
                }
                else if (m_flashOn)
                {
                    m_flashOn = false;
                    m_testTimeRemainingSec = 10;
                }
            }
        }
        break;

    case TestPattern::LongDurationWhite:
    case TestPattern::FullFramePeak:
        if (m_newTestSelected)
        {
            m_testTimeRemainingSec = 1800.0f; // 30 minutes
        }
        else
        {
            m_testTimeRemainingSec -= static_cast<float>(timer.GetElapsedSeconds());
            m_testTimeRemainingSec = std::max(0.0f, m_testTimeRemainingSec);
        }
        break;

    case TestPattern::TenPercentPeak:				// test 1.
    case TestPattern::TenPercentPeakMAX:			// test 1.MAX
        if (m_newTestSelected)
        {
            m_testTimeRemainingSec = 1800.0f;		// 30 minutes
        }
        else
        {
            m_testTimeRemainingSec -= static_cast<float>(timer.GetElapsedSeconds());
            m_testTimeRemainingSec = std::max(0.0f, m_testTimeRemainingSec);
        }
        break;


	case TestPattern::RiseFallTime:
		if (m_newTestSelected)
		{
			m_testTimeRemainingSec = 3.0f;
		}
		else
		{
			m_testTimeRemainingSec -= static_cast<float>(timer.GetElapsedSeconds());
			m_testTimeRemainingSec = std::max(0.0f, m_testTimeRemainingSec);
			if (m_testTimeRemainingSec <= 0.0001)
			{
				if (!m_flashOn)
				{
					m_flashOn = true;
					m_testTimeRemainingSec = 5;
				}
				else if (m_flashOn)
				{
					m_flashOn = false;
					m_testTimeRemainingSec = 5;
				}
			}
		}
		break;


	case TestPattern::XRiteColors:
	{
		if (m_newTestSelected)
		{
			m_testTimeRemainingSec = m_XRitePatchDisplayTime;
			m_XRitePatchAutoMode = false;           // v1.5 flag for when it auto animates
		}
		else
		{
			if (m_XRitePatchAutoMode)
			{
				m_testTimeRemainingSec -= static_cast<float>(timer.GetElapsedSeconds());
				if (m_testTimeRemainingSec < 0.f)
				{
					m_currentXRiteIndex += 1;
					m_currentXRiteIndex = (int)wrap((float)m_currentXRiteIndex, 0.f, NUMXRITECOLORS);	// Just wrap on each end <inclusive!>
					m_testTimeRemainingSec += m_XRitePatchDisplayTime;					// extend timer by one period
				}
			}
		}
	}

	case TestPattern::ActiveDimming: break;
	case TestPattern::ActiveDimmingDark: break;


    case TestPattern::BitDepthPrecision:
        // TODO: How exactly are we choosing this to correspond with expected banding?
        // We don't know what internal EOTF is being used by the display, so how
        // do we draw a ruler that matches with anything but sRGB?
        endColor = D2D1::ColorF(0.25f, 0.25f, 0.25f);
        break;

    case TestPattern::StaticGradient:
        endColor = m_gradientColor;
        break;

    case TestPattern::AnimatedGrayGradient:
        // Color channels vary between 0.0 and 2 * m_gradientEndPoint.
        endColor = D2D1::ColorF(
            m_gradientAnimationBase * sin(m_totalTime) + m_gradientAnimationBase,
            m_gradientAnimationBase * sin(m_totalTime) + m_gradientAnimationBase,
            m_gradientAnimationBase * sin(m_totalTime) + m_gradientAnimationBase);
        break;

    case TestPattern::AnimatedColorGradient:
        // Color channels vary between 0.0 and 2 * m_gradientEndPoint.
        endColor = D2D1::ColorF(
            m_gradientAnimationBase * sin(m_totalTime * 2.0f) + m_gradientAnimationBase,
            m_gradientAnimationBase * sin(m_totalTime * 1.0f) + m_gradientAnimationBase,
            m_gradientAnimationBase * sin(m_totalTime * 0.5f) + m_gradientAnimationBase);
        break;

    default:
        // Not all test patterns have animations, so not implemented is fine.
        break;
    }

    // TODO: This should go into a shared method for the gradient test patterns.
    D2D1_GRADIENT_STOP gradientStops[2];
    gradientStops[0].color = D2D1::ColorF(D2D1::ColorF::Black, 1);
    gradientStops[0].position = 0.0f;
    gradientStops[1].color = endColor;
    gradientStops[1].position = 1.0f;

    auto ctx = m_deviceResources->GetD2DDeviceContext();

    D2D1_BUFFER_PRECISION prec = D2D1_BUFFER_PRECISION_UNKNOWN;
    switch (m_deviceResources->GetBackBufferFormat())
    {
    case DXGI_FORMAT_B8G8R8A8_UNORM:
        prec = D2D1_BUFFER_PRECISION_8BPC_UNORM;
        break;

    case DXGI_FORMAT_R16G16B16A16_UNORM:
        prec = D2D1_BUFFER_PRECISION_16BPC_UNORM;
        break;

    case DXGI_FORMAT_R16G16B16A16_FLOAT:
        prec = D2D1_BUFFER_PRECISION_16BPC_FLOAT;
        break;

    default:
        DX::ThrowIfFailed(E_INVALIDARG);
        break;
    }

    ComPtr<ID2D1GradientStopCollection1> stopCollection;
    DX::ThrowIfFailed(ctx->CreateGradientStopCollection(
        gradientStops,
        ARRAYSIZE(gradientStops),
        D2D1_COLOR_SPACE_SRGB, // TODO: Why must I convert from sRGB to scRGB?
        D2D1_COLOR_SPACE_SCRGB,
        prec,
        D2D1_EXTEND_MODE_CLAMP,
        D2D1_COLOR_INTERPOLATION_MODE_PREMULTIPLIED, // No alpha, doesn't matter
        &stopCollection));

    auto rect = m_deviceResources->GetLogicalSize();

    DX::ThrowIfFailed(ctx->CreateLinearGradientBrush(
        D2D1::LinearGradientBrushProperties(
            D2D1::Point2F(0, 0),
            D2D1::Point2F(rect.right, 0)), // Assumes rect origin is 0,0?
        stopCollection.Get(),
        &m_gradientBrush));

    if (m_dxgiColorInfoStale)
    {
        UpdateDxgiColorimetryInfo();
    }
}

void Game::UpdateDxgiColorimetryInfo()
{
    // Output information is cached on the DXGI Factory. If it is stale we need to create
    // a new factory and re-enumerate the displays.
    auto d3dDevice = m_deviceResources->GetD3DDevice();

    ComPtr<IDXGIDevice3> dxgiDevice;
    DX::ThrowIfFailed(d3dDevice->QueryInterface(IID_PPV_ARGS(&dxgiDevice)));

    ComPtr<IDXGIAdapter> dxgiAdapter;
    DX::ThrowIfFailed(dxgiDevice->GetAdapter(&dxgiAdapter));

	dxgiAdapter->GetDesc(&m_adapterDesc);

    ComPtr<IDXGIFactory4> dxgiFactory;
    DX::ThrowIfFailed(dxgiAdapter->GetParent(IID_PPV_ARGS(&dxgiFactory)));

//    if (!dxgiFactory->IsCurrent())
    {
        DX::ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory)));
    }

    // Get information about the display we are presenting to.
    ComPtr<IDXGIOutput> output;
    auto sc = m_deviceResources->GetSwapChain();
    DX::ThrowIfFailed(sc->GetContainingOutput(&output));

    ComPtr<IDXGIOutput6> output6;
    output.As(&output6);

    DX::ThrowIfFailed(output6->GetDesc1(&m_outputDesc));

	// Get raw (not OS-modified) luminance data:
	DISPLAY_DEVICE device = {};
	device.cb = sizeof(device);

	DisplayMonitor foundMonitor{ nullptr };
	for (UINT deviceIndex = 0; EnumDisplayDevices(m_outputDesc.DeviceName, deviceIndex, &device, EDD_GET_DEVICE_INTERFACE_NAME); deviceIndex++)
	{
		if (device.StateFlags & DISPLAY_DEVICE_ACTIVE)
		{
			winrt::hstring hstr = winrt::to_hstring(device.DeviceID);
			foundMonitor = DisplayMonitor::FromInterfaceIdAsync(hstr).get();
			if (foundMonitor)
			{
				break;
			}
		}
	}

	if (!foundMonitor)
	{
	}

	winrt::Windows::Graphics::SizeInt32 dims;
	dims = foundMonitor.NativeResolutionInRawPixels();
	m_modeWidth = dims.Width;
	m_modeHeight = dims.Height;

	m_monitorName = foundMonitor.DisplayName();

	m_connectionKind = foundMonitor.ConnectionKind();
	m_physicalConnectorKind = foundMonitor.PhysicalConnector();
	// m_connectionDescriptorKind = foundMonitor.DisplayMonitorDescriptorKind();
    // foundMonitor.GetDescriptor( &m_connectionDescriptorKind );
	// I think this is how to get the raw EDID or DisplayID byte array.

	// set staticContrast test#5 to maxLuminance but clamped to 500nits.
	float maxNits = fmin(m_outputDesc.MaxLuminance, 500.f);
	if (CheckHDR_On())
		m_staticContrastPQValue = Apply2084(maxNits / 10000.f) * 1023.f;
	else
		m_staticContrastsRGBValue = (maxNits / 270.f) * 255.f;

	// save the raw (not OS-modified) luminance data:
	m_rawOutDesc.MaxLuminance = foundMonitor.MaxLuminanceInNits();
	m_rawOutDesc.MaxFullFrameLuminance = foundMonitor.MaxAverageFullFrameLuminanceInNits();
	m_rawOutDesc.MinLuminance = foundMonitor.MinLuminanceInNits();
	// TODO: Should also get color primaries...

	// get PQ code at MaxLuminance
	m_maxPQCode = (int) roundf(1023.0f*Apply2084(m_rawOutDesc.MaxLuminance / 10000.f));
//	m_maxPQCode = 1023;			// limit PQ code to valid range

	m_dxgiColorInfoStale = false;

    //	ACPipeline();
}

// reset metadata to default state (same as panel properties) so it need do no tone mapping
// Note: OS does this on boot and on app exit.
void Game::SetMetadataNeutral()
{

    m_Metadata.MaxContentLightLevel      = static_cast<UINT16>(m_rawOutDesc.MaxLuminance);
    m_Metadata.MaxFrameAverageLightLevel = static_cast<UINT16>(m_rawOutDesc.MaxFullFrameLuminance);
    m_Metadata.MaxMasteringLuminance     = static_cast<UINT>(m_rawOutDesc.MaxLuminance);
    m_Metadata.MinMasteringLuminance     = static_cast<UINT>(m_rawOutDesc.MinLuminance * 10000.0f);

	m_MetadataGamut = ColorGamut::GAMUT_Native;
	m_Metadata.RedPrimary[0]  = static_cast<UINT16>(m_outputDesc.RedPrimary[0]   * 50000.0f);
    m_Metadata.RedPrimary[1]  = static_cast<UINT16>(m_outputDesc.RedPrimary[1]   * 50000.0f);
    m_Metadata.GreenPrimary[0]= static_cast<UINT16>(m_outputDesc.GreenPrimary[0] * 50000.0f);
    m_Metadata.GreenPrimary[1]= static_cast<UINT16>(m_outputDesc.GreenPrimary[1] * 50000.0f);
    m_Metadata.BluePrimary[0] = static_cast<UINT16>(m_outputDesc.BluePrimary[0]  * 50000.0f);
    m_Metadata.BluePrimary[1] = static_cast<UINT16>(m_outputDesc.BluePrimary[1]  * 50000.0f);
    m_Metadata.WhitePoint[0]  = static_cast<UINT16>(m_outputDesc.WhitePoint[0]   * 50000.0f);
    m_Metadata.WhitePoint[1]  = static_cast<UINT16>(m_outputDesc.WhitePoint[1]   * 50000.0f);

    auto sc = m_deviceResources->GetSwapChain();
    DX::ThrowIfFailed(sc->SetHDRMetaData(DXGI_HDR_METADATA_TYPE_HDR10, sizeof(DXGI_HDR_METADATA_HDR10), &m_Metadata));
}

void Game::SetMetadata(float max, float avg, ColorGamut gamut)
{
    m_Metadata.MaxContentLightLevel = static_cast<UINT16>(max);
    m_Metadata.MaxFrameAverageLightLevel = static_cast<UINT16>(avg);
    m_Metadata.MaxMasteringLuminance = static_cast<UINT>(max);
    m_Metadata.MinMasteringLuminance = static_cast<UINT>(0.0 * 10000.0f);

    switch (gamut)
    {
    case GAMUT_Native:
		m_MetadataGamut = ColorGamut::GAMUT_Native;
		m_Metadata.RedPrimary[0]   = static_cast<UINT16>(m_outputDesc.RedPrimary[0]   * 50000.0f);
        m_Metadata.RedPrimary[1]   = static_cast<UINT16>(m_outputDesc.RedPrimary[1]   * 50000.0f);
        m_Metadata.GreenPrimary[0] = static_cast<UINT16>(m_outputDesc.GreenPrimary[0] * 50000.0f);
        m_Metadata.GreenPrimary[1] = static_cast<UINT16>(m_outputDesc.GreenPrimary[1] * 50000.0f);
        m_Metadata.BluePrimary[0]  = static_cast<UINT16>(m_outputDesc.BluePrimary[0]  * 50000.0f);
        m_Metadata.BluePrimary[1]  = static_cast<UINT16>(m_outputDesc.BluePrimary[1]  * 50000.0f);
		break;
    case GAMUT_sRGB:
		m_MetadataGamut = ColorGamut::GAMUT_sRGB;
		m_Metadata.RedPrimary[0]   = static_cast<UINT16>(primaryR_709.x * 50000.0f);
        m_Metadata.RedPrimary[1]   = static_cast<UINT16>(primaryR_709.y * 50000.0f);
        m_Metadata.GreenPrimary[0] = static_cast<UINT16>(primaryG_709.x * 50000.0f);
        m_Metadata.GreenPrimary[1] = static_cast<UINT16>(primaryG_709.y * 50000.0f);
        m_Metadata.BluePrimary[0]  = static_cast<UINT16>(primaryB_709.x * 50000.0f);
        m_Metadata.BluePrimary[1]  = static_cast<UINT16>(primaryB_709.y * 50000.0f);
        break;
    case GAMUT_Adobe:
		m_MetadataGamut = ColorGamut::GAMUT_Adobe;
		m_Metadata.RedPrimary[0]   = static_cast<UINT16>(primaryR_Adobe.x * 50000.0f);
        m_Metadata.RedPrimary[1]   = static_cast<UINT16>(primaryR_Adobe.y * 50000.0f);
        m_Metadata.GreenPrimary[0] = static_cast<UINT16>(primaryG_Adobe.x * 50000.0f);
        m_Metadata.GreenPrimary[1] = static_cast<UINT16>(primaryG_Adobe.y * 50000.0f);
        m_Metadata.BluePrimary[0]  = static_cast<UINT16>(primaryB_Adobe.x * 50000.0f);
        m_Metadata.BluePrimary[1]  = static_cast<UINT16>(primaryB_Adobe.y * 50000.0f);
		break;
    case GAMUT_DCIP3:
		m_MetadataGamut = ColorGamut::GAMUT_DCIP3;
		m_Metadata.RedPrimary[0]   = static_cast<UINT16>(primaryR_DCIP3.x * 50000.0f);
        m_Metadata.RedPrimary[1]   = static_cast<UINT16>(primaryR_DCIP3.y * 50000.0f);
        m_Metadata.GreenPrimary[0] = static_cast<UINT16>(primaryG_DCIP3.x * 50000.0f);
        m_Metadata.GreenPrimary[1] = static_cast<UINT16>(primaryG_DCIP3.y * 50000.0f);
        m_Metadata.BluePrimary[0]  = static_cast<UINT16>(primaryB_DCIP3.x * 50000.0f);
        m_Metadata.BluePrimary[1]  = static_cast<UINT16>(primaryB_DCIP3.y * 50000.0f);
		break;
    case GAMUT_BT2100:
		m_MetadataGamut = ColorGamut::GAMUT_BT2100;
		m_Metadata.RedPrimary[0]   = static_cast<UINT16>(primaryR_2020.x * 50000.0f);
        m_Metadata.RedPrimary[1]   = static_cast<UINT16>(primaryR_2020.y * 50000.0f);
        m_Metadata.GreenPrimary[0] = static_cast<UINT16>(primaryG_2020.x * 50000.0f);
        m_Metadata.GreenPrimary[1] = static_cast<UINT16>(primaryG_2020.y * 50000.0f);
        m_Metadata.BluePrimary[0]  = static_cast<UINT16>(primaryB_2020.x * 50000.0f);
        m_Metadata.BluePrimary[1]  = static_cast<UINT16>(primaryB_2020.y * 50000.0f);
		break;
    }

    m_Metadata.WhitePoint[0] = static_cast<UINT16>(D6500White.x * 50000.0f);
    m_Metadata.WhitePoint[1] = static_cast<UINT16>(D6500White.y * 50000.0f);

    auto sc = m_deviceResources->GetSwapChain();
    DX::ThrowIfFailed(sc->SetHDRMetaData(DXGI_HDR_METADATA_TYPE_HDR10, sizeof(DXGI_HDR_METADATA_HDR10), &m_Metadata));
}

// dump out the metadata to a string for display
void Game::PrintMetadata( ID2D1DeviceContext2* ctx, bool blackText /* = false */ )
{
	std::wstringstream text;
	// print luminance levels
	text << "MaxCLL: ";
	text << std::to_wstring((int)m_Metadata.MaxContentLightLevel);
	text << "  MaxFALL: ";
	text << std::to_wstring((int)m_Metadata.MaxFrameAverageLightLevel);
	text << "  MinCLL: ";
	text << std::to_wstring(m_Metadata.MinMasteringLuminance / 10000.0f);

	// print gamut in use
	text << "  Gamut: ";
	switch ( m_MetadataGamut )
	{
	case GAMUT_Native:
		text << "Native\n";
		break;
	case GAMUT_sRGB:
		text << "sRGB\n";
		break;
	case GAMUT_Adobe:
		text << "Adobe\n";
		break;
	case GAMUT_DCIP3:
		text << "DCI-P3\n";
		break;
	case GAMUT_BT2100:
		text << "bt.2100\n";
		break;
	}

	RenderText(ctx, m_largeFormat.Get(), text.str(), m_MetadataTextRect, blackText);
}

// dump out the metadata to a string for display
void Game::PrintTestingTier(ID2D1DeviceContext2* ctx, bool blackText /* = false */)
{
	std::wstringstream text;
	// print luminance levels

	text << GetTierName(m_testingTier);

	RenderText(ctx, m_largeFormat.Get(), text.str(), m_TestingTierTextRect, blackText);
}

void Game::GenerateTestPattern_StartOfTest(ID2D1DeviceContext2* ctx)
{
    auto fmt = m_deviceResources->GetBackBufferFormat();
    std::wstringstream text;

    if (m_newTestSelected) SetMetadataNeutral();

    text << m_appTitle;
    text << L"\n\nVersion 1.2 Build 15\n\n";
    //text << L"ALT-ENTER: Toggle fullscreen: all measurements should be made in fullscreen\n";
	text << L"->, PAGE DN:       Move to next test\n";
	text << L"<-, PAGE UP:        Move to previous test\n";
    text << L"NUMBER KEY:	Jump to test number\n";
    text << L"SPACE:		Hide text and target circle\n";
    text << L"C:		Start 60s cool-down\n";
    text << L"ALT-ENTER:	Toggle fullscreen\n";
    text << L"ESCAPE:		Exit fullscreen\n";
    text << L"ALT-F4:		Exit app\n";
    text << L"\nCopyright © VESA\nLast updated: " << __DATE__;
    text << L"\nIncludes Portrait X-Rite™ color technology\n";

    RenderText(ctx, m_largeFormat.Get(), text.str(), m_largeTextRect);

    if (m_showExplanatoryText)
    {

        std::wstringstream title;
        title << L"Home.   Start Screen\n" << m_hideTextString;

        RenderText(ctx, m_largeFormat.Get(), title.str(), m_testTitleRect);
    }
    m_newTestSelected = false;

}

bool Game::CheckHDR_On()
{
	// TODO this should query an HDR bit if one is available
	bool HDR_On = false;
	switch (m_outputDesc.ColorSpace)
	{
	case DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709: // sRGB
		break;

	case DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020: // HDR10 PC
		HDR_On = true;
		break;

	default:
		break;
	}

	return HDR_On;
}

void Game::GenerateTestPattern_ConnectionProperties(ID2D1DeviceContext2* ctx)
{
    if (m_newTestSelected) SetMetadataNeutral();

    std::wstringstream text;

	text << L"Render GPU: ";
	text << m_adapterDesc.Description;

	text << L"\nMonitor: ";
	text << m_monitorName.c_str();

	text << L"\nDisplay: ";
	WCHAR* DisplayName = wcsrchr(m_outputDesc.DeviceName, '\\');
	text << ++DisplayName;

	text << L"\nConnector: ";

	switch (m_connectionKind)
	{
	case DisplayMonitorConnectionKind::Internal:
		text << "Internal Panel ";
		break;
	case DisplayMonitorConnectionKind::Wired:
		text << "Wired ";
		break;
	case DisplayMonitorConnectionKind::Wireless:
		text << "Wireless";
		break;
	case DisplayMonitorConnectionKind::Virtual:
		text << "Virtual";
		break;
	default:
		text << "Error";
		break;
	}

	switch (m_physicalConnectorKind)
	{
	case DisplayMonitorPhysicalConnectorKind::Unknown:
		if (m_connectionKind != DisplayMonitorConnectionKind::Internal)
			text << "unknown";
		break;
	case DisplayMonitorPhysicalConnectorKind::HD15:
		text << "HD-15";
		break;
	case DisplayMonitorPhysicalConnectorKind::AnalogTV:
		text << "Analog TV";
		break;
	case DisplayMonitorPhysicalConnectorKind::Dvi:
		text << "DVI";
		break;
	case DisplayMonitorPhysicalConnectorKind::Hdmi:
		text << "HDMI";
		break;
	case DisplayMonitorPhysicalConnectorKind::Lvds:
		text << "LVDS";
		break;
	case DisplayMonitorPhysicalConnectorKind::Sdi:
		text << "SDI";
		break;
	case DisplayMonitorPhysicalConnectorKind::DisplayPort:
		text << "DisplayPort";
		break;
	default:
		text << "Error";
		break;
	}

#if 0 // TODO: apparently the method to return this does not exist in Windows.
	switch (m_connectionDescriptorKind)
	{
	case DisplayMonitorDescriptorKind::Edid:
		text << " with EDID";
		break;
	case DisplayMonitorDescriptorKind::DisplayId:
		text << " with DisplayID";
		break;
	default:
		text << " ";  // " Error"; 
		break;
	}
#endif

    text << L"\n\nConnection colorspace: [";
	text << std::to_wstring(m_outputDesc.ColorSpace);
	switch (m_outputDesc.ColorSpace)
	{
	case DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709: // sRGB
		text << L"] sRGB/SDR";
		break;

	case DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020: // HDR10 PC
		text << L"] HDR/advanced color ON";
		break;
	default:
		text << L"] Unknown";
		break;
	}

	DEVMODE DevNode;
	EnumDisplaySettingsW(NULL, ENUM_CURRENT_SETTINGS, &DevNode);
	m_displayFrequency = DevNode.dmDisplayFrequency;  // TODO: this only works on the iGPU!

	text << "\nResolution: " << m_modeWidth << " x " << m_modeHeight;
	text << " x " << std::to_wstring(m_outputDesc.BitsPerColor) << L"bits @ ";
	text << m_displayFrequency << L"Hz\n";

    bool HDR_On = CheckHDR_On();

	if (!HDR_On)
    {
        text << L"\nBefore starting tests, make sure \"HDR and Advanced color\"";
        text << L"\n is enabled in the Display settings panel.";
    }

#if 0		// these values are useful for HGIG, but not really for DisplayHDR
	// display properties of any tone mapper applied as determined from manual calibration screens
	if ( HDR_On )
	{
		text << L"\n\nSupported PQ Values - ";
		text << L"\nMax Effective Value: ";
		text << std::to_wstring((int)m_maxEffectivePQValue);
		text << L" (";
		text << std::to_wstring(Remove2084(m_maxEffectivePQValue/1023.0f)*10000.0f);
		text << L" nits)";

		text << L"\nMax FullFrame Value: ";
		text << std::to_wstring((int)m_maxFullFramePQValue);
		text << L" (";
		text << std::to_wstring(Remove2084(m_maxFullFramePQValue / 1023.0f)*10000.0f);
		text << L" nits)";

		text << L"\nMin Effective Value: ";
		text << std::to_wstring((int)m_minEffectivePQValue);
		text << L" (  ";
		text << std::to_wstring(Remove2084(m_minEffectivePQValue/1023.0f)*10000.0f);
		text << L" nits)";
	}
	else
	{
		text << L"\n\nSupported sRGB Values - ";
		text << L"\nMax Effective Value: ";
		text << std::to_wstring((int)m_maxEffectivesRGBValue);
		text << L" (";
		text << std::to_wstring(RemoveSRGBCurve(m_maxEffectivesRGBValue/255.0f)*80.0f);
		text << L" nits)";

		text << L"\nMax FullFrame Value: ";
		text << std::to_wstring((int)m_maxFullFramesRGBValue);
		text << L" (";
		text << std::to_wstring(RemoveSRGBCurve(m_maxFullFramesRGBValue/255.0f)*80.0f);
		text << L" nits)";

		text << L"\nMin Effective Value: ";
		text << std::to_wstring((int)m_minEffectivesRGBValue);
		text << L" (  ";
		text << std::to_wstring(RemoveSRGBCurve(m_minEffectivesRGBValue/255.0f)*80.0f);
		text << L" nits)";
	}
#endif

    RenderText(ctx, m_monospaceFormat.Get(), text.str(), m_largeTextRect);

    if (m_showExplanatoryText)
    {
        std::wstring title = L"Connection properties:\nPress SPACE to hide this text\n";
        RenderText(ctx, m_largeFormat.Get(), title, m_testTitleRect);
    }

    m_newTestSelected = false;
}

void Game::GenerateTestPattern_PanelCharacteristics(ID2D1DeviceContext2* ctx)
{
    if (m_newTestSelected) SetMetadataNeutral();

    std::wstringstream text;
	text << fixed << setw(9) << setprecision(2);

	// TODO: ensure these values are up to date.
	if (CheckForDefaults())
	{
		text << L"***WARNING: These are OS-provided defaults. Display did not provide any data.***\n";
	}
	// else
	{
		text << L"\nHDR Testing Tier: " << GetTierName(m_testingTier);
		text << L" - Change using Up/Down Arrow Keys" << L"\n";
	}
	
	
	//	text << m_outputDesc.DeviceName;

	text << L"\nBase Levels: (nits)";
    text << L"\n  Max peak luminance:          ";
	text << setprecision(0);
    text << m_rawOutDesc.MaxLuminance;
    text << L".\n  Max frame-average luminance: ";
    text << m_rawOutDesc.MaxFullFrameLuminance;
	text << L".\n  Min luminance:                 ";
	text << setprecision(4);
	text << m_rawOutDesc.MinLuminance;
	text << L"\nAdjusted Levels: (nits)";
	text << L"\n  Max peak luminance:          ";
	text << setprecision(0);
	text << m_outputDesc.MaxLuminance;
	text << L".\n  Max frame-average luminance: ";
	text << m_outputDesc.MaxFullFrameLuminance;
	text << L".\n  Min luminance:                 ";
	text << setprecision(4);
	text << m_outputDesc.MinLuminance;
	text << L"\nCurr. Brightness Slider Factor:  ";
	text << setprecision(2);
	text << BRIGHTNESS_SLIDER_FACTOR;

//  text << L"\nContrast ratio (peak/min):   ";
//  text << std::to_wstring(m_outputDesc.MaxLuminance / m_outputDesc.MinLuminance );
    text << L"\n";

    // Compute area of this device's gamut in uv coordinates

    // first, get primaries of display in 1931 xy coordinates
    float2 red_xy, grn_xy, blu_xy, wht_xy;
    red_xy = m_outputDesc.RedPrimary;
    grn_xy = m_outputDesc.GreenPrimary;
    blu_xy = m_outputDesc.BluePrimary;
    wht_xy = m_outputDesc.WhitePoint;

	text << L"\nCIE 1931          x         y";
    text << L"\nRed Primary  :  " << std::to_wstring(red_xy.x) << L"  " << std::to_wstring(red_xy.y);
    text << L"\nGreen Primary:  " << std::to_wstring(grn_xy.x) << L"  " << std::to_wstring(grn_xy.y);
    text << L"\nBlue Primary :  " << std::to_wstring(blu_xy.x) << L"  " << std::to_wstring(blu_xy.y);
    text << L"\nWhite Point  :  " << std::to_wstring(wht_xy.x) << L"  " << std::to_wstring(wht_xy.y);

    float gamutAreaDevice = ComputeGamutArea(red_xy, grn_xy, blu_xy);

    // rec.709/sRGB gamut area in uv coordinates
    float gamutAreasRGB = ComputeGamutArea(primaryR_709, primaryG_709, primaryB_709);

    // AdobeRGB gamut area in uv coordinates
    float GamutAreaAdobe = ComputeGamutArea(primaryR_Adobe, primaryG_Adobe, primaryB_Adobe);

    // DCI-P3 gamut area in uv coordinates
    float gamutAreaDCIP3 = ComputeGamutArea(primaryR_DCIP3, primaryG_DCIP3, primaryB_DCIP3);

    // BT.2020 gamut area in uv coordinates
    float gamutAreaBT2100 = ComputeGamutArea(primaryR_2020, primaryG_2020, primaryB_2020);

    // ACES gamut area in uv coordinates
    float gamutAreaACES = ComputeGamutArea(primaryR_ACES, primaryG_ACES, primaryB_ACES);

    const float gamutAreaHuman = 0.195f;	// TODO get this actual data!

    // Compute extent that this device gamut covers known popular ones:
    float coverageSRGB  = ComputeGamutCoverage(red_xy, grn_xy, blu_xy, primaryR_709,   primaryG_709,   primaryB_709);
    float coverageAdobe = ComputeGamutCoverage(red_xy, grn_xy, blu_xy, primaryR_Adobe, primaryG_Adobe, primaryB_Adobe);
    float coverageDCIP3 = ComputeGamutCoverage(red_xy, grn_xy, blu_xy, primaryR_DCIP3, primaryG_DCIP3, primaryB_DCIP3);
    float coverage2100  = ComputeGamutCoverage(red_xy, grn_xy, blu_xy, primaryR_2020,  primaryG_2020,  primaryB_2020);
    float coverageACES  = ComputeGamutCoverage(red_xy, grn_xy, blu_xy, primaryR_ACES,  primaryG_ACES,  primaryB_ACES);

    // display coverage values
    text << L"\n\nGamut Coverage based on reported primaries";
    text << L"\n        % sRGB : " << std::to_wstring(coverageSRGB*100.f);
    text << L"\n    % AdobeRGB : " << std::to_wstring(coverageAdobe*100.f);
    text << L"\n      % DCI-P3 : " << std::to_wstring(coverageDCIP3*100.f);
    text << L"\n     % BT.2100 : " << std::to_wstring(coverage2100*100.f);
    text << L"\n   % Human eye : " << std::to_wstring(gamutAreaDevice / gamutAreaHuman*100.f);

    // test code:
    float theory = gamutAreaDevice / gamutAreaBT2100;
    //	text << L"\n     %  Theory : " << std::to_wstring( theory*100.f);
    //	text << L"\n       % Error : " << std::to_wstring((coverage2100 - theory) / theory * 100.f);

    RenderText(ctx, m_monospaceFormat.Get(), text.str(), m_largeTextRect);

    if (m_showExplanatoryText)
    {
        std::wstring title = L"Reported Panel Characteristics\n" + m_hideTextString;
        RenderText(ctx, m_largeFormat.Get(), title, m_testTitleRect);

		PrintMetadata(ctx);
	}

    m_newTestSelected = false;

    // TODO should bail here if no valid data
}

void setBrightnessSliderPercent(UCHAR percent)
{
	HANDLE display = CreateFile(
		L"\\\\.\\LCD",
		(GENERIC_READ | GENERIC_WRITE),
		NULL,
		NULL,
		OPEN_EXISTING,
		0,
		NULL);

	if (display == INVALID_HANDLE_VALUE)
	{
		throw new std::runtime_error("Failed to open handle to display for setting brightness");
	}
	else
	{
		DWORD ret;
		DISPLAY_BRIGHTNESS displayBrightness{};
		displayBrightness.ucACBrightness = percent;
		displayBrightness.ucDCBrightness = percent;
		displayBrightness.ucDisplayPolicy = DISPLAYPOLICY_BOTH;

		bool result = !DeviceIoControl(
			display,
			IOCTL_VIDEO_SET_DISPLAY_BRIGHTNESS,
			&displayBrightness,
			sizeof(displayBrightness),
			NULL,
			0,
			&ret,
			NULL);

		if (result)
		{
			throw new std::runtime_error("Failed to set brightness");
		}
	}
}


void Game::GenerateTestPattern_ResetInstructions(ID2D1DeviceContext2* ctx)
{
	if (m_newTestSelected)
	{

		SetMetadataNeutral();

// This works but we don't know what to set it to.
// So it is commented out until we do
//		setBrightnessSliderPercent(33);
// EDID settings don't update afterwards anyway until next app run
	}

    if (m_showExplanatoryText)
    {
        std::wstring title = L"Start of performance tests\n" + m_hideTextString;
        RenderText(ctx, m_largeFormat.Get(), title, m_testTitleRect);
    }
    std::wstringstream text;
    text << L"For external displays, use their OSD to reset\n";
    text << L"all settings to the default or factory state.\n\n";

	text << L"Set any OS \'Brightness\' slider to factor 1.0 on previous screen.\n";
	text << L"Set the \'SDR color appearance\' slider to the leftmost point.\n";
	text << L"DO NOT CHANGE BRIGHTNESS SLIDERS AFTER APP START.\n\n";

	text << L"Be sure the \'Night light' setting is off.\n";

	text << L"Disable any Ambient Light Sensor or Adaptive Color capability.\n\n";

	text << L"The display should be at normal operating temperature.\n";
    text << L"  -This can take as much as 30 minutes for some panels.\n";

    text << L"  -Also note, ambient temperature may affect scores.\n\n";

    text << L"Use the following screen to check correct brightness levels";

    RenderText(ctx, m_largeFormat.Get(), text.str(), m_largeTextRect);
    m_newTestSelected = false;
}

void Game::DrawLogo(ID2D1DeviceContext2 *ctx, float c)
{
	ComPtr<ID2D1SolidColorBrush> centerBrush;
	DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF(c, c, c), &centerBrush));
	auto logSize = m_deviceResources->GetLogicalSize();
	float size = sqrt((logSize.right - logSize.left) * (logSize.bottom - logSize.top));
	size = size * sqrtf(0.10);  // dimension of a square of 10% screen area
	size = size * 0.3333333f;

	float2 center;
	center.x = (logSize.right - logSize.left)*0.50f;
	center.y = (logSize.bottom - logSize.top)*0.50f;

	D2D1_RECT_F centerSquare1 =
	{
		center.x - size * 1.03f,
		center.y - size * 1.03f,
		center.x - size * 0.03f,
		center.y - size * 0.03f
	};
	ctx->FillRectangle(&centerSquare1, centerBrush.Get());

	D2D1_RECT_F centerSquare2 =
	{
		center.x + size * 0.03f,
		center.y - size * 1.03f,
		center.x + size * 1.03f,
		center.y - size * 0.03f
	};
	ctx->FillRectangle(&centerSquare2, centerBrush.Get());

	D2D1_RECT_F centerSquare3 =
	{
		center.x - size * 1.03f,
		center.y + size * 0.03f,
		center.x - size * 0.03f,
		center.y + size * 1.03f
	};
	ctx->FillRectangle(&centerSquare3, centerBrush.Get());

	D2D1_RECT_F centerSquare4 =
	{
		center.x + size * 0.03f,
		center.y + size * 0.03f,
		center.x + size * 1.03f,
		center.y + size * 1.03f
	};
	ctx->FillRectangle(&centerSquare4, centerBrush.Get());
}

void Game::GenerateTestPattern_CalibrateMaxEffectiveValue(ID2D1DeviceContext2* ctx) // Detect white crush (MaxTML)
{
	// outer box
	float outerNits = m_outputDesc.MaxLuminance;
	float avg = 600.0f;
	if (m_newTestSelected) SetMetadata(outerNits, avg, GAMUT_Native);
	float c = nitstoCCCS(outerNits);

	ComPtr<ID2D1SolidColorBrush> outerBrush;
	DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF(c, c, c), &outerBrush));

	auto logSize = m_deviceResources->GetLogicalSize();
	float size = sqrt((logSize.right - logSize.left) * (logSize.bottom - logSize.top));
	size = size * sqrtf(0.10);  // dimension of a square of 10% screen area
	float2 center;
	center.x = (logSize.right - logSize.left) * 0.50f;
	center.y = (logSize.bottom - logSize.top) * 0.50f;
	D2D1_RECT_F outerSquare =
	{
		center.x - size * 0.50f,
		center.y - size * 0.50f,
		center.x + size * 0.50f,
		center.y + size * 0.50f
	};

	ctx->FillRectangle(&outerSquare, outerBrush.Get());

	// inner boxes
	float nits = 0;
	if (CheckHDR_On())
	{
		nits = Remove2084(m_maxEffectivePQValue / 1023.0f) * 10000.0f;
		c = nitstoCCCS(nits);
	}
	else
	{
		c = m_maxEffectivesRGBValue / 255.0f;
		nits = RemoveSRGBCurve(c) * 80.0f;
	}
	ComPtr<ID2D1SolidColorBrush> centerBrush;
	DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF(c, c, c), &centerBrush));

	size = size * 0.3333333f;
	D2D1_RECT_F centerSquare1 =
	{
		center.x - size * 1.03f,
		center.y - size * 1.03f,
		center.x - size * 0.03f,
		center.y - size * 0.03f
	};
	ctx->FillRectangle(&centerSquare1, centerBrush.Get());

	D2D1_RECT_F centerSquare2 =
	{
		center.x + size * 0.03f,
		center.y - size * 1.03f,
		center.x + size * 1.03f,
		center.y - size * 0.03f
	};
	ctx->FillRectangle(&centerSquare2, centerBrush.Get());

	D2D1_RECT_F centerSquare3 =
	{
		center.x - size * 1.03f,
		center.y + size * 0.03f,
		center.x - size * 0.03f,
		center.y + size * 1.03f
	};
	ctx->FillRectangle(&centerSquare3, centerBrush.Get());

	D2D1_RECT_F centerSquare4 =
	{
		center.x + size * 0.03f,
		center.y + size * 0.03f,
		center.x + size * 1.03f,
		center.y + size * 1.03f
	};
	ctx->FillRectangle(&centerSquare4, centerBrush.Get());

	if( m_showExplanatoryText )
	{
		std::wstringstream title;
		title << L"Calibrate Max Tone Mapped Luminance Value: ";
		if (CheckHDR_On())
		{
			title << L"\nHDR10: ";
			title << static_cast<unsigned int>(m_maxEffectivePQValue);
			title << L"  Nits: ";
			title << nits;
		}
		else
		{
			title << L"\nsRGB: ";
			title << static_cast<unsigned int>(m_maxEffectivesRGBValue);
			title << L"  Nits: ";
			title << nits;
		}

		title << L"\nAdjust brightness using Up/Down arrows";
		title << L"\n  until inner boxes just barely disappear";
		RenderText(ctx, m_largeFormat.Get(), title.str(), m_testTitleRect);

		PrintMetadata(ctx);
	}

	m_newTestSelected = false;

}

void Game::GenerateTestPattern_CalibrateMaxFullFrameValue(ID2D1DeviceContext2 * ctx) // Verify MaxFALL (MaxFF TML)
{
	// set up background (clear full screen)
	float backNits = m_outputDesc.MaxFullFrameLuminance;
	float avg = backNits;
	if (m_newTestSelected) SetMetadata(backNits, avg, GAMUT_Native);
	float c = nitstoCCCS(backNits);
	ComPtr<ID2D1SolidColorBrush> backBrush;
	DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF(c, c, c), &backBrush));
	auto logSize = m_deviceResources->GetLogicalSize();
	ctx->FillRectangle(&logSize, backBrush.Get());
	float2 center;
	center.x = (logSize.right - logSize.left)*0.50f;
	center.y = (logSize.bottom - logSize.top)*0.50f;
	float size = sqrt((logSize.right - logSize.left) * (logSize.bottom - logSize.top));

	// inner boxes
	float nits = 0;
	if (CheckHDR_On())
	{
		nits = Remove2084(m_maxFullFramePQValue / 1023.0f)*10000.0f;
		c = nitstoCCCS(nits);
	}
	else
	{
		c = m_maxFullFramesRGBValue / 255.0f;
		nits = RemoveSRGBCurve(c)*270.0f;
	}
	ComPtr<ID2D1SolidColorBrush> centerBrush;
	DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF(c, c, c), &centerBrush));

	size = size * 0.333333333f;
	size = size * 0.333333333f;
	D2D1_RECT_F centerSquare1 =
	{
		center.x - size * 1.03f,
		center.y - size * 1.03f,
		center.x - size * 0.03f,
		center.y - size * 0.03f
	};
	ctx->FillRectangle(&centerSquare1, centerBrush.Get());

	D2D1_RECT_F centerSquare2 =
	{
		center.x + size * 0.03f,
		center.y - size * 1.03f,
		center.x + size * 1.03f,
		center.y - size * 0.03f
	};
	ctx->FillRectangle(&centerSquare2, centerBrush.Get());

	D2D1_RECT_F centerSquare3 =
	{
		center.x - size * 1.03f,
		center.y + size * 0.03f,
		center.x - size * 0.03f,
		center.y + size * 1.03f
	};
	ctx->FillRectangle(&centerSquare3, centerBrush.Get());

	D2D1_RECT_F centerSquare4 =
	{
		center.x + size * 0.03f,
		center.y + size * 0.03f,
		center.x + size * 1.03f,
		center.y + size * 1.03f
	};
	ctx->FillRectangle(&centerSquare4, centerBrush.Get());

	if (m_showExplanatoryText)
	{
		std::wstringstream title;
		title << L"Calibrate Max Full Frame Luminance Value: ";
		if (CheckHDR_On())
		{
			title << L"\nHDR10: ";
			title << static_cast<unsigned int>(m_maxFullFramePQValue);
			title << L"  Nits: ";
			title << nits;
		}
		else
		{
			title << L"\nsRGB: ";
			title << static_cast<unsigned int>(m_maxEffectivesRGBValue);
			title << L"  Nits: ";
			title << nits;
		}

		title << L"\nAdjust brightness using Up/Down arrows";
		title << L"\n  until inner boxes just barely disappear";
		RenderText(ctx, m_largeFormat.Get(), title.str(), m_testTitleRect, true);

		PrintMetadata(ctx, true);
	}

	m_newTestSelected = false;

}


void Game::GenerateTestPattern_CalibrateMinEffectiveValue(ID2D1DeviceContext2 * ctx) // Detect Black Crush
{
	// set up background
	float backNits = 2.0f;
	float avg = backNits*0.90f;
	if (m_newTestSelected) SetMetadata(backNits, avg, GAMUT_Native);
	float c = nitstoCCCS(backNits);
	ComPtr<ID2D1SolidColorBrush> backBrush;
	DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF(c, c, c), &backBrush));
	auto logSize = m_deviceResources->GetLogicalSize();
	ctx->FillRectangle(&logSize, backBrush.Get());

	// draw 10% square in pure black
	float outerNits = 0.0f;
	c = nitstoCCCS(outerNits);
	ComPtr<ID2D1SolidColorBrush> outerBrush;
	DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF(c, c, c), &outerBrush));

	float size = sqrt((logSize.right - logSize.left) * (logSize.bottom - logSize.top));
	size = size*sqrtf(0.10);  // dimension of a square of 10% screen area
	float2 center;
	center.x = (logSize.right - logSize.left)*0.50f;
	center.y = (logSize.bottom - logSize.top)*0.50f;
	D2D1_RECT_F outerSquare =
	{
		center.x - size*0.50f,
		center.y - size*0.50f,
		center.x + size*0.50f,
		center.y + size*0.50f
	};

	ctx->FillRectangle(&outerSquare, outerBrush.Get());

	// inner boxes
	float nits = 0;
	if (CheckHDR_On())
	{
		nits = Remove2084(m_minEffectivePQValue / 1023.0f)*10000.0f;
		c = nitstoCCCS(nits);
	}
	else
	{
		c = m_minEffectivesRGBValue / 255.0f;
		nits = RemoveSRGBCurve(c)*80.0f;
	}

	ComPtr<ID2D1SolidColorBrush> centerBrush;
	DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF(c, c, c), &centerBrush));

	size = size*0.3333333f;
	D2D1_RECT_F centerSquare1 =
	{
		center.x - size*1.03f,
		center.y - size*1.03f,
		center.x - size*0.03f,
		center.y - size*0.03f
	};
	ctx->FillRectangle(&centerSquare1, centerBrush.Get());

	D2D1_RECT_F centerSquare2 =
	{
		center.x + size*0.03f,
		center.y - size*1.03f,
		center.x + size*1.03f,
		center.y - size*0.03f
	};
	ctx->FillRectangle(&centerSquare2, centerBrush.Get());

	D2D1_RECT_F centerSquare3 =
	{
		center.x - size*1.03f,
		center.y + size*0.03f,
		center.x - size*0.03f,
		center.y + size*1.03f
	};
	ctx->FillRectangle(&centerSquare3, centerBrush.Get());

	D2D1_RECT_F centerSquare4 =
	{
		center.x + size*0.03f,
		center.y + size*0.03f,
		center.x + size*1.03f,
		center.y + size*1.03f
	};
	ctx->FillRectangle(&centerSquare4, centerBrush.Get());

	if (m_showExplanatoryText)
	{
		std::wstringstream title;
		title << L"Calibrate Min Tone-Mapped Luminance Level: ";
		if (CheckHDR_On())
		{
			title << L"\nHDR10: ";
			title << static_cast<unsigned int>(m_minEffectivePQValue);
			title << L"  Nits: ";
			title << nits;
		}
		else
		{
			title << L"\nsRGB: ";
			title << static_cast<unsigned int>(m_minEffectivesRGBValue);
			title << L"  Nits: ";
			title << nits;
		}

		title << L"\nAdjust brightness using Up/Down arrows";
		title << L"\n  until inner boxes just barely disappear";
		RenderText(ctx, m_largeFormat.Get(), title.str(), m_testTitleRect);

		PrintMetadata(ctx);
	}

	m_newTestSelected = false;
}


void Game::GenerateTestPattern_PQLevelsInNits(ID2D1DeviceContext2 * ctx)		// 
{


    if (m_newTestSelected) SetMetadata(10000.0, 180.0, GAMUT_Native);

    // Grayscale patches generated using the following table:
    // PQ Code	Nits	CCCS
    // 0		0		0.0f
    // 153		1		0.0125f
    // 192		2		0.025f
    // 206		2.5		0.03125f
    // 253		5		0.0625f
    // 306		10		0.125f
    // 364		20		0.25f
    // 428		40		0.5f
    // 496		80		1.0f
    // 567		160		2.0f
    // 641		320		4.0f
    // 719		640		8.0f
    // 767		1000	12.5f
    // 844		2000	25.0f
    // 920		4000	50.0f
    // 1023		10000	125.0f
    // (PQ code values are not exact, reference is nits).

    ComPtr<ID2D1SolidColorBrush> nits0, nits1, nits2, nits2_5, nits5, nits10, nits20, nits40, nits80;
    ComPtr<ID2D1SolidColorBrush> nits160, nits320, nits640, nits1000, nits2000, nits4000, nits10000;

	float c = 0.f;
    DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF(c,c,c), &nits0));
	c = 0.0125f / BRIGHTNESS_SLIDER_FACTOR;
    DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF(c,c,c), &nits1));
	c = 0.025f / BRIGHTNESS_SLIDER_FACTOR;
    DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF(c,c,c), &nits2));
	c = 0.03125f / BRIGHTNESS_SLIDER_FACTOR;
	DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF(c,c,c), &nits2_5));
	c = 0.0625f / BRIGHTNESS_SLIDER_FACTOR;
	DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF(c,c,c), &nits5));
	c = 0.125f / BRIGHTNESS_SLIDER_FACTOR;
	DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF(c,c,c), &nits10));
	c = 0.25f / BRIGHTNESS_SLIDER_FACTOR;
	DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF(c,c,c), &nits20));
	c = 0.5f / BRIGHTNESS_SLIDER_FACTOR;
	DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF(c,c,c), &nits40));
	c = 1.0f / BRIGHTNESS_SLIDER_FACTOR;
	DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF(c,c,c), &nits80));
	c = 2.0f / BRIGHTNESS_SLIDER_FACTOR;
	DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF(c,c,c), &nits160));
	c = 4.0f / BRIGHTNESS_SLIDER_FACTOR;
	DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF(c,c,c), &nits320));
	c = 8.0f / BRIGHTNESS_SLIDER_FACTOR;
	DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF(c,c,c), &nits640));
	c = 12.5f / BRIGHTNESS_SLIDER_FACTOR;
	DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF(c,c,c), &nits1000));
	c = 25.0f / BRIGHTNESS_SLIDER_FACTOR;
	DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF(c,c,c), &nits2000));
	c = 50.0f / BRIGHTNESS_SLIDER_FACTOR;
	DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF(c,c,c), &nits4000));
	c = 125.f / BRIGHTNESS_SLIDER_FACTOR;
	DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF(c,c,c), &nits10000));

    // We only render small patches of each color to limit power consumption.
    auto rect = m_deviceResources->GetLogicalSize();
    auto w = (rect.right - rect.left) / 4.0f; // Divide screen in 4x4 grid.
    auto h = (rect.bottom - rect.top) / 4.0f;
    auto w1 = w * 0.2f; // Left/top edge of each patch offset.
    auto h1 = h * 0.2f;
    auto w2 = w * 0.8f; // Right/bottom edge of each patch offset.
    auto h2 = h * 0.8f;

    ctx->FillRectangle(D2D1::RectF(w * 0.0f + w1, h * 0.0f + h1, w * 0.0f + w2, h * 0.0f + h2), nits0.Get());
    ctx->FillRectangle(D2D1::RectF(w * 1.0f + w1, h * 0.0f + h1, w * 1.0f + w2, h * 0.0f + h2), nits1.Get());
    ctx->FillRectangle(D2D1::RectF(w * 2.0f + w1, h * 0.0f + h1, w * 2.0f + w2, h * 0.0f + h2), nits2.Get());
    ctx->FillRectangle(D2D1::RectF(w * 3.0f + w1, h * 0.0f + h1, w * 3.0f + w2, h * 0.0f + h2), nits2_5.Get());
    ctx->FillRectangle(D2D1::RectF(w * 0.0f + w1, h * 1.0f + h1, w * 0.0f + w2, h * 1.0f + h2), nits5.Get());
    ctx->FillRectangle(D2D1::RectF(w * 1.0f + w1, h * 1.0f + h1, w * 1.0f + w2, h * 1.0f + h2), nits10.Get());
    ctx->FillRectangle(D2D1::RectF(w * 2.0f + w1, h * 1.0f + h1, w * 2.0f + w2, h * 1.0f + h2), nits20.Get());
    ctx->FillRectangle(D2D1::RectF(w * 3.0f + w1, h * 1.0f + h1, w * 3.0f + w2, h * 1.0f + h2), nits40.Get());
    ctx->FillRectangle(D2D1::RectF(w * 0.0f + w1, h * 2.0f + h1, w * 0.0f + w2, h * 2.0f + h2), nits80.Get());
    ctx->FillRectangle(D2D1::RectF(w * 1.0f + w1, h * 2.0f + h1, w * 1.0f + w2, h * 2.0f + h2), nits160.Get());
    ctx->FillRectangle(D2D1::RectF(w * 2.0f + w1, h * 2.0f + h1, w * 2.0f + w2, h * 2.0f + h2), nits320.Get());
    ctx->FillRectangle(D2D1::RectF(w * 3.0f + w1, h * 2.0f + h1, w * 3.0f + w2, h * 2.0f + h2), nits640.Get());
    ctx->FillRectangle(D2D1::RectF(w * 0.0f + w1, h * 3.0f + h1, w * 0.0f + w2, h * 3.0f + h2), nits1000.Get());
    ctx->FillRectangle(D2D1::RectF(w * 1.0f + w1, h * 3.0f + h1, w * 1.0f + w2, h * 3.0f + h2), nits2000.Get());
    ctx->FillRectangle(D2D1::RectF(w * 2.0f + w1, h * 3.0f + h1, w * 2.0f + w2, h * 3.0f + h2), nits4000.Get());
    ctx->FillRectangle(D2D1::RectF(w * 3.0f + w1, h * 3.0f + h1, w * 3.0f + w2, h * 3.0f + h2), nits10000.Get());

    if (m_showExplanatoryText)
    {

        RenderText(ctx, m_largeFormat.Get(), L"PQ:0    Nits:0    ", { w * 0.0f + w1, h * 1.0f - h1, 200.0f, 30.0f });
        RenderText(ctx, m_largeFormat.Get(), L"PQ:153  Nits:1    ", { w * 1.0f + w1, h * 1.0f - h1, 200.0f, 30.0f });
        RenderText(ctx, m_largeFormat.Get(), L"PQ:193  Nits:2    ", { w * 2.0f + w1, h * 1.0f - h1, 200.0f, 30.0f });
        RenderText(ctx, m_largeFormat.Get(), L"PQ:206  Nits:2.5  ", { w * 3.0f + w1, h * 1.0f - h1, 200.0f, 30.0f });
        RenderText(ctx, m_largeFormat.Get(), L"PQ:254  Nits:5    ", { w * 0.0f + w1, h * 2.0f - h1, 200.0f, 30.0f });
        RenderText(ctx, m_largeFormat.Get(), L"PQ:307  Nits:10   ", { w * 1.0f + w1, h * 2.0f - h1, 200.0f, 30.0f });
        RenderText(ctx, m_largeFormat.Get(), L"PQ:366  Nits:20   ", { w * 2.0f + w1, h * 2.0f - h1, 200.0f, 30.0f });
        RenderText(ctx, m_largeFormat.Get(), L"PQ:429  Nits:40   ", { w * 3.0f + w1, h * 2.0f - h1, 200.0f, 30.0f });
        RenderText(ctx, m_largeFormat.Get(), L"PQ:497  Nits:80   ", { w * 0.0f + w1, h * 3.0f - h1, 200.0f, 30.0f });
        RenderText(ctx, m_largeFormat.Get(), L"PQ:568  Nits:160  ", { w * 1.0f + w1, h * 3.0f - h1, 200.0f, 30.0f });
        RenderText(ctx, m_largeFormat.Get(), L"PQ:643  Nits:320  ", { w * 2.0f + w1, h * 3.0f - h1, 200.0f, 30.0f });
        RenderText(ctx, m_largeFormat.Get(), L"PQ:719  Nits:640  ", { w * 3.0f + w1, h * 3.0f - h1, 200.0f, 30.0f });
        RenderText(ctx, m_largeFormat.Get(), L"PQ:767  nits:1000 ", { w * 0.0f + w1, h * 4.0f - h1, 200.0f, 30.0f });
        RenderText(ctx, m_largeFormat.Get(), L"PQ:844  nits:2000 ", { w * 1.0f + w1, h * 4.0f - h1, 200.0f, 30.0f });
        RenderText(ctx, m_largeFormat.Get(), L"PQ:920  nits:4000 ", { w * 2.0f + w1, h * 4.0f - h1, 200.0f, 30.0f });
        RenderText(ctx, m_largeFormat.Get(), L"PQ:1023 nits:10000", { w * 3.0f + w1, h * 4.0f - h1, 250.0f, 30.0f });

        std::wstring title = L"PQ/ST 2084 levels in nits\n" + m_hideTextString;
        RenderText(ctx, m_largeFormat.Get(), title, m_testTitleRect);

		PrintMetadata(ctx);
	}
    m_newTestSelected = false;

}

void Game::GenerateTestPattern_WarmUp(ID2D1DeviceContext2 * ctx)
{
    float nits = 180.0f; // warm up level
    if (m_newTestSelected) SetMetadata(nits, nits, GAMUT_Native);
    float c = nitstoCCCS(nits)/BRIGHTNESS_SLIDER_FACTOR;

    ComPtr<ID2D1SolidColorBrush> peakBrush;
    DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF(c, c, c), &peakBrush));

    auto logSize = m_deviceResources->GetLogicalSize();
    ctx->FillRectangle(&logSize, peakBrush.Get());

    if (m_showExplanatoryText)
    {
        std::wstringstream title;
		title << L"Warm-Up: ";
		title << fixed << setw(8) << setprecision(2);
		if (0.0f != m_testTimeRemainingSec)
        {
            title << m_testTimeRemainingSec;
            title << L" seconds remaining";
            title << L"\nNits: ";
			title << nits;
            title << L"  HDR10: ";
			title << setprecision(0);
            title << Apply2084(c*80.f / 10000.f) * 1023.f;
            title << L"\n" << m_hideTextString;
        }
        else
        {
            title << L" done.";
        }

        RenderText(ctx, m_largeFormat.Get(), title.str(), m_testTitleRect, true);

		PrintMetadata(ctx, true);
    }
    m_newTestSelected = false;
}

// this is invoked via the C key
// it just draws a black screen for 60seconds.
void Game::GenerateTestPattern_Cooldown(ID2D1DeviceContext2 * ctx)
{
    if (m_newTestSelected) SetMetadataNeutral();

    if (m_showExplanatoryText)
    {
        std::wstringstream title;
        title << L"Cool-down: ";
        if (0.0f != m_testTimeRemainingSec)
        {
            title << static_cast<unsigned int>(m_testTimeRemainingSec);
            title << L" seconds remaining\n" << m_hideTextString;
        }
        else
        {
            title << L" done.";
        }

        RenderText(ctx, m_largeFormat.Get(), title.str(), m_testTitleRect);

		PrintMetadata(ctx);
	}
    m_newTestSelected = false;

}

#define randf()((float)rand()/(float(RAND_MAX)+1.0f))
float randf_s()
{
	unsigned int number;
	errno_t err;
	err = rand_s( &number );
	return (float)number / ((float)UINT_MAX + 1);
}

#define JITTER_RADIUS 10.0f
void Game::GenerateTestPattern_TenPercentPeak(ID2D1DeviceContext2* ctx) //********************** 1.a
{
	float patchPct = PATCHPCT;			// patch percentage of screen area
	auto logSize = m_deviceResources->GetLogicalSize();
	std::wstringstream title;

	// "tone map" PQ limit of 10k nits down to panel maxLuminance in CCCS
	float nits = m_outputDesc.MaxLuminance;
	float avg = nits * patchPct;								// 10% screen area
	if (m_newTestSelected) {
		SetMetadata(nits, avg, GAMUT_Native);
	}

	// Draw the background noise effect
	auto rsc = m_testPatternResources[TestPattern::TenPercentPeak];
	if (!rsc.effectIsValid)
	{
		title << L"\nERROR: " << rsc.effectShaderFilename << L" is missing\n";
	}
	else
	{
		float TierMaxLum = GetTierLuminance(m_testingTier);
		float APL = TierMaxLum * 0.02174f;							// convert to nits based on current tier
		float limit = 100.0f;

		// ******* Arguments to this call are not type checked  *********
		// ******* DO NOT PASS A DOUBLE HERE (EVEN LITERAL!) *** ELSE FAIL ************* 
		rsc.d2dEffect->SetValueByName(L"APL", APL );		// ****** MUST BE FLOAT!		// in nits
		rsc.d2dEffect->SetValueByName(L"Clamp", limit );									// nits
		rsc.d2dEffect->SetValueByName(L"iTime", m_totalTime );								// seconds

		ctx->DrawImage(rsc.d2dEffect.Get());
	}

	// draw the center rectangle
	float c;
    c = nitstoCCCS(nits);
    ComPtr<ID2D1SolidColorBrush> centerBrush;
    DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF(c, c, c), &centerBrush));

	float size = sqrt((logSize.right - logSize.left) * (logSize.bottom - logSize.top));
	size = size * sqrtf(patchPct);  // dimensions for a square of this % screen area
	float2 center;
	center.x = (logSize.right - logSize.left) * 0.50f;
	center.y = (logSize.bottom - logSize.top) * 0.50f;

	float dpi = m_deviceResources->GetDpi();
	float2 jitter;
	float radius = JITTER_RADIUS * dpi / 96.0f;
	do {
		jitter.x = radius*(randf_s()*2.f - 1.f);
		jitter.y = radius*(randf_s()*2.f - 1.f);
	}
	while( (jitter.x*jitter.x + jitter.y*jitter.y) > radius);

	// Apply jitter
	center = center + jitter;

	D2D1_RECT_F centerRect =
	{
		center.x - size * 0.50f,
		center.y - size * 0.50f,
		center.x + size * 0.50f,
		center.y + size * 0.50f
	};
	ctx->FillRectangle(&centerRect, centerBrush.Get());

    if (m_showExplanatoryText)
    {
		float fRad = 0.5f * m_snoodDiam / 25.4f * dpi * 1.2f;      // radius of dia 40mm -> inches -> dips
		float2 center = float2(logSize.right*0.5f, logSize.bottom*0.5f);

		D2D1_ELLIPSE ellipse =
		{
			D2D1::Point2F(center.x, center.y),
			fRad, fRad
		};
		ctx->DrawEllipse(&ellipse, m_redBrush.Get(), 1 );

        std::wstringstream title;
		title << fixed << setw(8) << setprecision(2);
		title << L"1.a Peak Luminance @ ";
		title << patchPct*100.f << L"% screen area\nWait before taking measurements : ";
        title << m_testTimeRemainingSec;
        title << L" seconds remaining";
        title << L"\nNits: ";
        title << nits*BRIGHTNESS_SLIDER_FACTOR;
        title << L"  HDR10: ";
		title << setprecision(0);
        title << Apply2084( c*80.f * BRIGHTNESS_SLIDER_FACTOR / 10000.f) * 1023.f;
		title << L"\n - Change Tier using Up/Down arrow keys";
        title << L"\n" << m_hideTextString;

        RenderText(ctx, m_largeFormat.Get(), title.str(), m_testTitleRect);

		PrintMetadata(ctx);
		PrintTestingTier(ctx);
	}
    m_newTestSelected = false;

}

// aka 1.b from v1.0
void Game::GenerateTestPattern_TenPercentPeakMAX(ID2D1DeviceContext2 * ctx) //**************** 1.MAX
{
    // MAX version uses 10,000 nits as peak
    float nits = 10000.0f;		// max PQ value
    float avg =   1000.0f;		// maxFALL

	if (m_newTestSelected) {
		SetMetadata(nits, avg, GAMUT_Native);
		srand(314159);
	}

	float c = nitstoCCCS(nits);

	float dpi = m_deviceResources->GetDpi();
    auto logSize = m_deviceResources->GetLogicalSize();
	std::wstringstream title;

	// Draw the background noise effect
	auto rsc = m_testPatternResources[TestPattern::TenPercentPeak];
	if (!rsc.effectIsValid)
	{
		title << L"\nERROR: " << rsc.effectShaderFilename << L" is missing\n";
	}
	else
	{
		float TierMaxLum = GetTierLuminance(m_testingTier);
		float APL = TierMaxLum * 0.022222222f;												// convert to nits based on current tier
		float limit = 400.0f;
		// ******* Arguments to this call are not type checked  *********
		// ******* DO NOT PASS A DOUBLE HERE (EVEN LITERAL!) *** ELSE FAIL ************* 
		rsc.d2dEffect->SetValueByName(L"APL", APL);		// ****** MUST BE FLOAT!			// average luminance in nits
		rsc.d2dEffect->SetValueByName(L"Clamp", limit );									// nits
		rsc.d2dEffect->SetValueByName(L"iTime", m_totalTime);								// seconds

		ctx->DrawImage(rsc.d2dEffect.Get());
	}

	// draw the center white square of the correct intensity
	ComPtr<ID2D1SolidColorBrush> peakBrush;
	DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF(c, c, c), &peakBrush));

	float patchPct = PATCHPCT;		// patch percentage of screen area
	float size = sqrt((logSize.right - logSize.left) * (logSize.bottom - logSize.top));
	size = size * sqrtf(patchPct);  // dimensions for a square of this % screen area

	float2 center;
	center.x = (logSize.right - logSize.left) * 0.50f;
	center.y = (logSize.bottom - logSize.top) * 0.50f;

	float2 jitter;
	float radius = JITTER_RADIUS * dpi / 96.0f;
	do {
		jitter.x = radius*(randf_s()*2.f - 1.f);
		jitter.y = radius*(randf_s()*2.f - 1.f);
	} while ((jitter.x*jitter.x + jitter.y*jitter.y) > radius );

	// Apply jitter
	center = center + jitter;

	D2D1_RECT_F tenPercentRect =
	{
		center.x - size * 0.50f,
		center.y - size * 0.50f,
		center.x + size * 0.50f,
		center.y + size * 0.50f
	};
    ctx->FillRectangle(&tenPercentRect, peakBrush.Get());

    if (m_showExplanatoryText)
    {
//		float fRad = sqrt((logSize.right - logSize.left) * (logSize.bottom - logSize.top)*0.04f);	// 4% screen area colorimeter box
		float fRad = 0.5f * m_snoodDiam / 25.4f * dpi * 1.2f;      // radius of dia 40mm -> inches -> dips

		float2 center = float2(logSize.right*0.5f, logSize.bottom*0.5f);

		D2D1_ELLIPSE ellipse =
		{
			D2D1::Point2F(center.x, center.y),
			fRad, fRad
		};
		ctx->DrawEllipse(&ellipse, m_redBrush.Get(), 2 );

		title << fixed << setw(8) << setprecision(2);
		title << L"1.b Peak Luminance MAX @ ";
		title << patchPct*100.f << L"% screen area\nWait before taking measurements: ";
        title << m_testTimeRemainingSec;
        title << L" seconds remaining";
        title << L"\nNits: ";
        title << nits;
        title << L"  HDR10: ";
		title << setprecision(0);
        title << Apply2084(c*80.f / 10000.f) * 1023.f;
		title << L"\n - Change Tier using Up/Down arrow keys";
        title << L"\n" << m_hideTextString;

        RenderText(ctx, m_largeFormat.Get(), title.str(), m_testTitleRect);

		PrintMetadata(ctx);
		PrintTestingTier(ctx);
	}
    m_newTestSelected = false;

}

void Game::GenerateTestPattern_FlashTest(ID2D1DeviceContext2 * ctx) //************************** 2.a
{

    // We "tone map" down to the maxLuminance reported by panel
    float nits = m_outputDesc.MaxLuminance;
    float avg = nits * 0.1f;
    if (m_newTestSelected) SetMetadata(nits, avg, GAMUT_Native);
    float c = nitstoCCCS(nits);

    ComPtr<ID2D1SolidColorBrush> peakFFWhite, blackBrush;
    DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF(c, c, c), &peakFFWhite));
    DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.0f, 0.0f), &blackBrush));

    std::wstringstream title;
	title << fixed << setw(8) << setprecision(2);

    if (m_flashOn)
    {
        title << "2.a Flash Test On\n";
        ctx->FillRectangle(m_deviceResources->GetLogicalSize(), peakFFWhite.Get());
    }
    else
    {
        title << "2.a Flash Test Off\n";
        ctx->FillRectangle(m_deviceResources->GetLogicalSize(), blackBrush.Get());
    }

    if (m_showExplanatoryText)
    {
        // TODO make this show 10ths of a second.
        title << static_cast<unsigned int>(m_testTimeRemainingSec);
        title << L" seconds remaining\nTests peak power throughput rendering at reported peak level";
        title << L"\nNits: ";
        title << nits*BRIGHTNESS_SLIDER_FACTOR;
        title << L"  HDR10: ";
		title << setprecision(0);
        title << Apply2084(c*80.f * BRIGHTNESS_SLIDER_FACTOR / 10000.f) * 1023.f;
        title << L"\n" << m_hideTextString;

        RenderText(ctx, m_largeFormat.Get(), title.str(), m_testTitleRect, m_flashOn);

		PrintMetadata(ctx, m_flashOn);
	}
    m_newTestSelected = false;
}

void Game::GenerateTestPattern_FlashTestMAX(ID2D1DeviceContext2 * ctx) //********************* 2.MAX
{
    // MAX version shoots for 10,000 nits
    float nits = 10000.0f;		// Max PQ Value
    float avg  =  1000.0f;		// MaxFALL
    if (m_newTestSelected) SetMetadata(nits, avg, GAMUT_Native);
    float c = nitstoCCCS(nits);

    ComPtr<ID2D1SolidColorBrush> peakFFWhite, blackBrush;
    DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF(c, c, c), &peakFFWhite));
    DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.0f, 0.0f), &blackBrush));

    std::wstringstream title;
	title << fixed << setw(8) << setprecision(2);

    if (m_flashOn)
    {
        title << "2.b MAX Flash Test On\n";
        ctx->FillRectangle(m_deviceResources->GetLogicalSize(), peakFFWhite.Get());
    }
    else
    {
        title << "2.b MAX Flash Test Off\n";
        ctx->FillRectangle(m_deviceResources->GetLogicalSize(), blackBrush.Get());
    }

    if (m_showExplanatoryText)
    {
        // TODO make this show 10ths of a second.
        title << static_cast<unsigned int>(m_testTimeRemainingSec);
        title << L" seconds remaining\nTests peak power throughput rendering MAX PQ";
        title << L"\nNits: ";
        title << nits;
        title << L"  HDR10: ";
		title << setprecision(0);
        title << Apply2084(c*80.f / 10000.f) * 1023;
        title << L"\n" << m_hideTextString;

        RenderText(ctx, m_largeFormat.Get(), title.str(), m_testTitleRect, m_flashOn);

		PrintMetadata(ctx, m_flashOn);
	}
    m_newTestSelected = false;

}

void Game::GenerateTestPattern_LongDurationWhite(ID2D1DeviceContext2 * ctx) //******************* 3.
{
    float nits = m_outputDesc.MaxLuminance;
    float avg = nits;								// Full frame means 100% of Max.
    if (m_newTestSelected)
		SetMetadata(nits, avg, GAMUT_Native);		// Effectively same as calling SetMetadataNeutral();
    float c = nitstoCCCS(nits);						// This test is for MaxFALL

    ComPtr<ID2D1SolidColorBrush> peakFFWhite;
    DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF(c, c, c), &peakFFWhite));

    ctx->FillRectangle(m_deviceResources->GetLogicalSize(), peakFFWhite.Get());

    if (m_showExplanatoryText)
    {

        std::wstringstream title;
		title << fixed << setw(8) << setprecision(2);

        title << L"3.a Full-frame white for 30 minutes: ";
        if (0.0f != m_testTimeRemainingSec)
        {
            title << static_cast<unsigned int>(m_testTimeRemainingSec);
            title << L" seconds remaining\nTests cooling solution by rendering reported MaxFALL";
            title << L"\nNits: ";
            title << nits*BRIGHTNESS_SLIDER_FACTOR;
            title << L"  HDR10: ";
			title << setprecision(0);
            title << Apply2084(c*80.f*BRIGHTNESS_SLIDER_FACTOR / 10000.f) * 1023.f;
            title << L"\n" << m_hideTextString;
        }
        else
        {
            title << L" done.";
        }

        RenderText(ctx, m_largeFormat.Get(), title.str(), m_testTitleRect, true);
		PrintMetadata(ctx, true);
	}
    m_newTestSelected = false;

}

void Game::GenerateTestPattern_FullFramePeak(ID2D1DeviceContext2 * ctx)	//******************** 3.MAX
{
    // Renders 10,000 nits. This is 125.0f in CCCS or 1023 HDR10 PQ.
    float nits = 10000.0f;
    float avg  = 10000.0f;   // full frame
    if (m_newTestSelected) SetMetadata(nits, avg, GAMUT_Native);
    float c = nitstoCCCS(nits)/BRIGHTNESS_SLIDER_FACTOR;

    ComPtr<ID2D1SolidColorBrush> peakBrush;
    DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF(c, c, c), &peakBrush));

	// fill entire screen
    ctx->FillRectangle(&m_deviceResources->GetLogicalSize(), peakBrush.Get());

    if (m_showExplanatoryText)
    {

        //	ComPtr<ID2D1SolidColorBrush> blackBrush;
        //	DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.0f, 0.0f), &blackBrush));
        //	ctx->FillRectangle(m_testTitleRect, blackBrush.Get());

        std::wstringstream title;
		title << fixed << setw(8) << setprecision(2);

        title << L"3.b Full-frame white for 30 minutes: ";
        if (0.0f != m_testTimeRemainingSec)
        {
            title << static_cast<unsigned int>(m_testTimeRemainingSec);
            title << L" seconds remaining";
            title << L"\nRenders HDR10 max (10,000 nits)";
            title << L"\nNits: ";
            title << nits;
            title << L"  HDR10: ";
			title << setprecision(0);
            title << 1023.f;
            title << L"\n" << m_hideTextString;
        }
        else
        {
            title << L" done.";
        }
        RenderText(ctx, m_largeFormat.Get(), title.str(), m_testTitleRect, true);

		PrintMetadata(ctx, true);
	}
    m_newTestSelected = false;

}

// Original 4-corner box test from v1.0
void Game::GenerateTestPattern_BlackLevelHdrCorners(ID2D1DeviceContext2* ctx) //**************** 4. legacy
{
	// Renders 10,000 nits. This is 125.0f CCCS or 1024 HDR10.
	ComPtr<ID2D1SolidColorBrush> cornerBrush;

	// Compute box intensity based on EDID
	float nits = m_outputDesc.MaxLuminance; // set to value from EDID ("tone map")
	if (nits > 600.0f)
		nits = 600.0f;                      // clamp to max of 600.0 per spec (712)
	if (nits < 400.0f)
		nits = 400.0f;						// clamp to min of 400.0 per spec (668)

	float avg = nits * 0.1f;                // 10% screen area
	if (m_newTestSelected) SetMetadata(nits, avg, GAMUT_Native);

	float c = nitstoCCCS(nits);
	DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF(c, c, c), &cornerBrush));

	auto logSize = m_deviceResources->GetLogicalSize();
	FLOAT fSize = sqrt(2.5f / 100.0f); // % screen area of each corner box

	D2D1_RECT_F upperLeftRect =
	{
		(logSize.right - logSize.left) * 0.0f,
		(logSize.bottom - logSize.top) * 0.0f,
		(logSize.right - logSize.left) * fSize,
		(logSize.bottom - logSize.top) * fSize
	};


	D2D1_RECT_F upperRightRect =
	{
		(logSize.right - logSize.left) * (1.0f - fSize),
		(logSize.bottom - logSize.top) * 0.0f,
		(logSize.right - logSize.left) * 1.0f,
		(logSize.bottom - logSize.top) * fSize
	};

	D2D1_RECT_F lowerLeftRect =
	{
		(logSize.right - logSize.left) * 0.0f,
		(logSize.bottom - logSize.top) * (1.0f - fSize),
		(logSize.right - logSize.left) * fSize,
		(logSize.bottom - logSize.top) * 1.0f
	};

	D2D1_RECT_F lowerRightRect =
	{
		(logSize.right - logSize.left) * (1.0f - fSize),
		(logSize.bottom - logSize.top) * (1.0f - fSize),
		(logSize.right - logSize.left) * 1.0f,
		(logSize.bottom - logSize.top) * 1.0f
	};

	ctx->FillRectangle(&upperLeftRect, cornerBrush.Get());
	ctx->FillRectangle(&upperRightRect, cornerBrush.Get());
	ctx->FillRectangle(&lowerLeftRect, cornerBrush.Get());
	ctx->FillRectangle(&lowerRightRect, cornerBrush.Get());

	// Everything below this point should be hidden for actual measurements.
	if (m_showExplanatoryText)
	{
		float fRad = sqrt((logSize.right - logSize.left) * (logSize.bottom - logSize.top) * 0.04f);	// 4% screen area colorimeter box
		float2 center = float2(logSize.right * 0.5f, logSize.bottom * 0.5f);

		D2D1_RECT_F centerRect =
		{
			center.x - fRad * 0.5f,
			center.y - fRad * 0.5f,
			center.x + fRad * 0.5f,
			center.y + fRad * 0.5f
		};
		ctx->DrawRectangle(centerRect, m_whiteBrush.Get());
#if 0
		D2D1_ELLIPSE ellipse =
		{
			D2D1::Point2F(center.x, center.y),
			fRad * 0.5f,
			fRad * 0.5f
		};
		ctx->DrawEllipse(&ellipse, m_redBrush.Get());
#endif

		std::wstringstream title;
		title << fixed << setw(8) << setprecision(2);

		title << L"4. Corner test for HDR black level\nMeasure in the center box";
		title << L"\nNits: ";
		title << nits * BRIGHTNESS_SLIDER_FACTOR;
		title << L"  HDR10: ";
		title << setprecision(0);
		title << Apply2084(c * 80.f * BRIGHTNESS_SLIDER_FACTOR / 10000.f) * 1023.f;
		title << L"\n" << m_hideTextString;

		// Shift title text to the right to avoid the corner.
		D2D1_RECT_F textRect = m_testTitleRect;
		textRect.left = (logSize.right - logSize.left) * fSize;
		RenderText(ctx, m_largeFormat.Get(), title.str(), textRect);

		PrintMetadata(ctx);
	}
	m_newTestSelected = false;

}

#if 1
void Game::GenerateTestPattern_DualCornerBox(ID2D1DeviceContext2* ctx)	//************************ 4.
{
	// Renders 10,000 nits. This is 125.0f CCCS or 1024 HDR10.
	ComPtr<ID2D1SolidColorBrush> cornerBrush;

	// Compute box intensity based on EDID
	float nits = m_outputDesc.MaxLuminance; // set to value from EDID ("tone map")
	float avg = nits * 0.1f;                // 10% screen area
	if (m_newTestSelected) SetMetadata(nits, avg, GAMUT_Native);

	float c = nitstoCCCS(nits);
	DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF(c, c, c), &cornerBrush));

	auto logSize = m_deviceResources->GetLogicalSize();
	float fSize = sqrt((logSize.right - logSize.left) * (logSize.bottom - logSize.top) * 0.05f);  // 5% screen area corner box
	D2D1_RECT_F upperRightRect =
	{
		logSize.right - fSize,
		0.0f,
		logSize.right,
		fSize
	};
	ctx->FillRectangle(&upperRightRect, cornerBrush.Get());

	D2D1_RECT_F lowerLeftRect =
	{
		0.0f,
		logSize.bottom - fSize,
		fSize,
		logSize.bottom
	};
	ctx->FillRectangle(&lowerLeftRect, cornerBrush.Get());

	// Everything below this point should be hidden for actual measurements.
	if (m_showExplanatoryText)
	{
		float dpi = m_deviceResources->GetDpi();

//		float fRad = sqrt((logSize.right - logSize.left) * (logSize.bottom - logSize.top) * 0.04f);	// 4% screen area colorimeter box
		float fRad = 0.5f * m_snoodDiam / 25.4f * dpi * 1.2f;      // radius of dia 27mm -> inches -> dips

		float2 center = float2(logSize.right * 0.5f, logSize.bottom * 0.5f);

		D2D1_ELLIPSE ellipse =
		{
			D2D1::Point2F(center.x, center.y),
			fRad, fRad
		};
		ctx->DrawEllipse(&ellipse, m_redBrush.Get());


		std::wstringstream title;
		title << fixed << setw(8) << setprecision(2);

		title << L"4. Corner test for Total Contrast\nMeasure in the center box";
		title << L"\nNits: ";
		title << nits * BRIGHTNESS_SLIDER_FACTOR;
		title << L"  HDR10: ";
		title << setprecision(0);
		title << Apply2084(c * 80.f * BRIGHTNESS_SLIDER_FACTOR / 10000.f) * 1023.f;
		title << L"\n" << m_hideTextString;

		// Shift title text to the right to avoid the corner.
		D2D1_RECT_F textRect = m_testTitleRect;
		RenderText(ctx, m_largeFormat.Get(), title.str(), textRect);

		PrintMetadata(ctx);
	}
	m_newTestSelected = false;

}

#else
/* How this should work:
	select PQ codes for test from CTS doc
	convert to nits for metadata
	convert to CCCS color for brush
	be sure to scale this down so that brightness control has no effect.
*/

void Game::GenerateTestPattern_DualCornerBox(ID2D1DeviceContext2 * ctx)	//************************ 4.
{
	// from CTS doc
	float PQCode = 668;
	if (m_testingTier > TestingTier::DisplayHDR400)
		PQCode =712;

	float nits = Remove2084( PQCode / 1023.0f) * 10000.0f;		// go to linear space

	float avg = nits * 0.1f;                // 10% screen area
	if (m_newTestSelected) SetMetadata(nits, avg, GAMUT_Native);
	// TODO: metadata needs to handle brightness slider factor

	ComPtr<ID2D1SolidColorBrush> cornerBrush;
	float c = nitstoCCCS(nits / BRIGHTNESS_SLIDER_FACTOR);		// scale by 80 and slider
	DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF(c, c, c), &cornerBrush));

	auto logSize = m_deviceResources->GetLogicalSize();
	float fSize = sqrt((logSize.right - logSize.left) * (logSize.bottom - logSize.top)*0.05f );  // 5% screen area corner box
	D2D1_RECT_F upperRightRect =
	{
		logSize.right - fSize,
		0.0f,
		logSize.right,
		fSize
	};
	ctx->FillRectangle(&upperRightRect, cornerBrush.Get());

	D2D1_RECT_F lowerLeftRect =
	{
		0.0f,
		logSize.bottom - fSize,
		fSize,
		logSize.bottom
	};
	ctx->FillRectangle(&lowerLeftRect, cornerBrush.Get());

	// Everything below this point should be hidden for actual measurements.
	if (m_showExplanatoryText)
	{
		float fRad = sqrt((logSize.right - logSize.left) * (logSize.bottom - logSize.top)*0.04f);	// 4% screen area colorimeter box
		float2 center = float2(logSize.right*0.5f, logSize.bottom*0.5f);

		D2D1_ELLIPSE ellipse =
		{
			D2D1::Point2F(center.x, center.y),
			fRad * 0.35f,						// TODO: make target diameter 27mm using DPI data
			fRad * 0.35f
		};
		ctx->DrawEllipse( &ellipse, m_redBrush.Get());


		std::wstringstream title;
		title << fixed << setw(8) << setprecision(2);

		title << L"4. Corner test for Total Contrast\nMeasure in the center box";
		title << L"\nNits: ";
		title << nits;
		title << L"  HDR10: ";
		title << setprecision(0);
		title << PQCode;
		title << L"\n" << m_hideTextString;

		// Shift title text to the right to avoid the corner.
		D2D1_RECT_F textRect = m_testTitleRect;
		RenderText(ctx, m_largeFormat.Get(), title.str(), textRect);

		PrintMetadata(ctx);
	}
	m_newTestSelected = false;

}
#endif

void Game::DrawChecker6x4( ID2D1DeviceContext2* ctx, float colorL, float colorR = -1.f)
{
	ComPtr<ID2D1SolidColorBrush> whiteBrush;			// brush for the "white" color
	ComPtr<ID2D1SolidColorBrush> rightBrush;			// brush for the "white" color on right half of screen

	//get a brush of the right white:
	DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF(colorL, colorL, colorL), &whiteBrush));
	if (colorR > 0)
		DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF(colorR, colorR, colorR), &rightBrush));

	// draw the "white" boxes on the black background
	D2D1_RECT_F logSize = m_deviceResources->GetLogicalSize();
	const int nCols = 6;
	const int nRows = 4;
	float width = (logSize.right - logSize.left);
	float height = (logSize.bottom - logSize.top);

	float2 step;
	step.x = width / nCols;
	step.y = height / nRows;
	for (int jRow = 0; jRow < nRows; jRow++)
	{
		for (int iCol = 0; iCol < nCols; iCol++)
		{
			if ((iCol + jRow) & 0x01)
			{  
				D2D1_RECT_F rect =
				{
					iCol * step.x,
					jRow * step.y,
					(iCol + 1) * step.x,
					(jRow + 1) * step.y
				};
				if ( ( iCol*step.x < (0.5f * width - 5.f) ) || (colorR < 0) )	// draw left half in other color if valid
					ctx->FillRectangle(&rect, whiteBrush.Get());
				else if (colorR >= 0)
					ctx->FillRectangle(&rect, rightBrush.Get());
			}
		}
	}

	// add any cross-hairs needed:
	if (m_showExplanatoryText)
	{
		float dpi = m_deviceResources->GetDpi();

		float fRad = 0.5f * m_snoodDiam / 25.4f * dpi * 1.2f;      // radius of dia 40mm -> inches -> dips
		float2 center = float2(logSize.right * 0.5f, logSize.bottom * 0.5f);

		D2D1_ELLIPSE ellipse;
		ellipse =
		{
			D2D1::Point2F(center.x + step.x * 0.5f, center.y + step.y * 0.5f),
			fRad, fRad
		};
		ctx->DrawEllipse(&ellipse, m_redBrush.Get(), 2.2f );
		ellipse =
		{
			D2D1::Point2F(center.x + step.x * 0.5f, center.y - step.y * 0.5f),
			fRad, fRad
		};
		ctx->DrawEllipse(&ellipse, m_redBrush.Get());
		ellipse =
		{
			D2D1::Point2F(center.x - step.x * 0.5f, center.y - step.y * 0.5f),
			fRad, fRad
		};
		ctx->DrawEllipse(&ellipse, m_redBrush.Get(), 2.2f );
		ellipse =
		{
			D2D1::Point2F(center.x - step.x * 0.5f, center.y + step.y * 0.5f),
			fRad, fRad
		};
		ctx->DrawEllipse(&ellipse, m_redBrush.Get());

		// draw crosshairs
		ctx->DrawLine(
			D2D1::Point2F(center.x - step.x, center.y - step.y * 0.5f),		// top horizontal line
			D2D1::Point2F(center.x + step.x, center.y - step.y * 0.5f),
			m_redBrush.Get(),
			2
		);
		ctx->DrawLine(
			D2D1::Point2F(center.x - step.x, center.y + step.y * 0.5f),		// bottom horizontal line
			D2D1::Point2F(center.x + step.x, center.y + step.y * 0.5f),
			m_redBrush.Get(),
			2
		);
		ctx->DrawLine(
			D2D1::Point2F(center.x - step.x*0.5f, center.y - step.y ),		// left vertical
			D2D1::Point2F(center.x - step.x*0.5f, center.y + step.y ),
			m_redBrush.Get(),
			2
		);
		ctx->DrawLine(
			D2D1::Point2F(center.x + step.x*0.5f, center.y - step.y ),		// right vertical
			D2D1::Point2F(center.x + step.x*0.5f, center.y + step.y ),
			m_redBrush.Get(),
			2
		);


	}
}

void Game::DrawChecker4x3(ID2D1DeviceContext2* ctx, float colorL, float colorR = -1.f)
{
	ComPtr<ID2D1SolidColorBrush> whiteBrush;			// brush for the "white" color
	ComPtr<ID2D1SolidColorBrush> rightBrush;			// brush for the "white" color on right half of screen

	//get a brush of the right white:
	DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF(colorL, colorL, colorL), &whiteBrush));
	if (colorR > 0)
		DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF(colorR, colorR, colorR), &rightBrush));

	// draw the "white" boxes on the black background
	D2D1_RECT_F logSize = m_deviceResources->GetLogicalSize();
	const int nCols = 4;
	const int nRows = 3;
	float width = (logSize.right - logSize.left);
	float height = (logSize.bottom - logSize.top);

	float centerWidth = roundf(0.48f * width * 2.f / 3.f);				// eqn 5-1 in DHDR 1.1 SCR VP235H App. A
	float sideWidth = floorf(width / 2.f) - centerWidth;				// eqn 5-2
	float edgeHeight = floorf(0.26f * height);						// eqn 5-3
	float centerHeight = height - 2.f * edgeHeight;					// eqn 5-4

	float2 step[5];
	step[0].x = 0.f;
	step[1].x = sideWidth;
	step[2].x = sideWidth + centerWidth;
	step[3].x = width - sideWidth;
	step[4].x = width;

	step[0].y = 0.f;
	step[1].y = edgeHeight;
	step[2].y = edgeHeight + centerHeight;
	step[3].y = height;

	for (int jRow = 0; jRow < nRows; jRow++)
	{
		for (int iCol = 0; iCol < nCols; iCol++)
		{
			if ( !((iCol + jRow) & 0x01) )
			{
				D2D1_RECT_F rect =
				{
					step[iCol].x,
					step[jRow].y,
					step[iCol + 1].x,
					step[jRow + 1].y
				};
				if ((iCol * step[iCol].x < (0.5 * width - 5.)) || (colorR < 0))	// draw left half in other color if valid
					ctx->FillRectangle(&rect, whiteBrush.Get());
				else if (colorR >= 0)
					ctx->FillRectangle(&rect, rightBrush.Get());
			}
		}
	}

	// add any cross-hairs needed:
	if (m_showExplanatoryText)
	{
		float dpi = m_deviceResources->GetDpi();
		float fRad = 0.5f * m_snoodDiam / 25.4f * dpi * 1.2f;      // radius of dia -> inches -> dips
		float2 center;

		center.y = height * 0.5f;
		center.x = sideWidth + centerWidth * 0.5f;
		D2D1_ELLIPSE ellipse;
		ellipse =
		{
			D2D1::Point2F(center.x, center.y),
			fRad, fRad
		};
		ctx->DrawEllipse(&ellipse, m_redBrush.Get(), 2.5f );
		center.x = sideWidth + centerWidth * 1.5f;
		ellipse =
		{
			D2D1::Point2F(center.x, center.y),
			fRad, fRad
		};
		ctx->DrawEllipse(&ellipse, m_redBrush.Get() );

		ctx->DrawLine(
			D2D1::Point2F( sideWidth,		height*0.5f ),							// horizontal line
			D2D1::Point2F( width-sideWidth, height*0.5f ),
			m_redBrush.Get(),
			2
		);
		ctx->DrawLine(
			D2D1::Point2F(sideWidth + centerWidth * 0.5f, edgeHeight),				// left vertical
			D2D1::Point2F(sideWidth + centerWidth * 0.5f, height-edgeHeight ),
			m_redBrush.Get(),
			2
		);
		ctx->DrawLine(
			D2D1::Point2F(sideWidth + centerWidth * 1.5f, edgeHeight),				// right vertical
			D2D1::Point2F(sideWidth + centerWidth * 1.5f, height - edgeHeight ),
			m_redBrush.Get(),
			1
		);

	}

}

void Game::DrawChecker4x3n(ID2D1DeviceContext2* ctx, float colorL, float colorR = -1.f )
{
	ComPtr<ID2D1SolidColorBrush> whiteBrush;			// brush for the "white" color
	ComPtr<ID2D1SolidColorBrush> rightBrush;			// brush for the "white" color on right half of screen

	//get a brush of the correct white:
	DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF(colorL, colorL, colorL), &whiteBrush));
	if ( colorR > 0)
		DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF(colorR, colorR, colorR), &rightBrush));

	// draw the "white" boxes on the black background
	D2D1_RECT_F logSize = m_deviceResources->GetLogicalSize();
	const int nCols = 4;
	const int nRows = 3;
	float width  = (logSize.right - logSize.left);
	float height = (logSize.bottom - logSize.top);

	float centerWidth = roundf( 0.48f * width * 2.f / 3.f );				// eqn 5-1 in DHDR 1.1 SCR VP235H App. A
	float sideWidth  = floorf( width/2.f ) - centerWidth;				// eqn 5-2
	float edgeHeight = floorf(0.26f * height);							// eqn 5-3
	float centerHeight = height - 2.f * edgeHeight;						// eqn 5-4

	float2 step[5];
	step[0].x =  0.;
	step[1].x = sideWidth;
	step[2].x = sideWidth + centerWidth;
	step[3].x = width - sideWidth;
	step[4].x = width;

	step[0].y = 0.;
	step[1].y = edgeHeight;
	step[2].y = edgeHeight + centerHeight;
	step[3].y = height;

	for (int jRow = 0; jRow < nRows; jRow++)
	{
		for (int iCol = 0; iCol < nCols; iCol++)
		{
			if ((iCol + jRow) & 0x01)
			{
				D2D1_RECT_F rect =
				{
					step[iCol].x,
					step[jRow].y,
					step[iCol+1].x,
					step[jRow+1].y
				};
				if ((iCol * step[iCol].x < (0.5 * width - 5.)) || (colorR < 0))	// draw left half in other color if valid
					ctx->FillRectangle(&rect, whiteBrush.Get());
				else if (colorR >= 0)
					ctx->FillRectangle(&rect, rightBrush.Get());
			}
		}
	}

	if (m_showExplanatoryText)
	{
		float dpi = m_deviceResources->GetDpi();
		float fRad = 0.5f * m_snoodDiam / 25.4f * dpi * 1.2f;      // radius of dia 27mm -> inches -> dips
		float2 center;

		center.y = height * 0.5f;
		center.x = sideWidth + centerWidth * 0.5f;
		D2D1_ELLIPSE ellipse;
		ellipse =
		{
			D2D1::Point2F(center.x, center.y),
			fRad, fRad
		};
		ctx->DrawEllipse(&ellipse, m_redBrush.Get());
		center.x = sideWidth + centerWidth * 1.5f;
		ellipse =
		{
			D2D1::Point2F(center.x, center.y),
			fRad, fRad
		};
		ctx->DrawEllipse(&ellipse, m_redBrush.Get(), 2.5f);

		ctx->DrawLine(
			D2D1::Point2F(sideWidth, height * 0.5f),									// horizontal line
			D2D1::Point2F(width - sideWidth, height * 0.5f),
			m_redBrush.Get(),
			2
		);
		ctx->DrawLine(
			D2D1::Point2F(sideWidth + centerWidth * 0.5f, edgeHeight),					// left vertical
			D2D1::Point2F(sideWidth + centerWidth * 0.5f, height - edgeHeight),
			m_redBrush.Get(),
			1
		);
		ctx->DrawLine(
			D2D1::Point2F(sideWidth + centerWidth * 1.5f, edgeHeight),					// right vertical
			D2D1::Point2F(sideWidth + centerWidth * 1.5f, height - edgeHeight),
			m_redBrush.Get(),
			2
		);
	}

}

void Game::GenerateTestPattern_StaticContrastRatio(ID2D1DeviceContext2 * ctx)       //**************** 5.
{
	float color, nits;
	if (CheckHDR_On())
	{
		nits = Remove2084(m_staticContrastPQValue/1023.0f) * 10000.0f;
		color = nitstoCCCS( nits );
	}
	else
	{
		color = m_staticContrastsRGBValue /255.0f;
		nits = RemoveSRGBCurve( color ) * 80.0f;
	}

	float avg = nits*0.50f;								// half of peak
	if (m_newTestSelected) SetMetadata(nits, avg, GAMUT_Native);

	switch (m_checkerboard)
	{
	case Checkerboard::Cb6x4:
		DrawChecker6x4(ctx, color);
		break;

	case Checkerboard::Cb4x3:
		DrawChecker4x3(ctx, color);
		break;

	case Checkerboard::Cb4x3not:
		DrawChecker4x3n(ctx, color);
		break;
	}

	// Everything below this point should be hidden during actual measurements.
	if (m_showExplanatoryText)
	{
		std::wstringstream title;
		title << fixed << setw(8) << setprecision(2);

		title << L"5. Static Contrast Ratio\nMeasure black vs white";
		title << L"\nNits: ";
		title << nits*BRIGHTNESS_SLIDER_FACTOR;
		if (CheckHDR_On())
		{
			title << L"  HDR10: ";
			title << setprecision(0);
			title << Apply2084(color*80.f*BRIGHTNESS_SLIDER_FACTOR / 10000.f) * 1023.f;
//			title << m_staticContrastPQValue;
		}
		else
		{
			title << L"   sRGB: ";
			title << setprecision(0);
			title << m_staticContrastsRGBValue;
		}

		title << L" -adjust using up/down arrows\n";
		title << L"Select checkerboard using LessThan < or GreaterThan > keys\n";
		title << L"\n" << m_hideTextString;

		RenderText(ctx, m_largeFormat.Get(), title.str(), m_testTitleRect );

		PrintMetadata(ctx);
	}
	m_newTestSelected = false;

}

void Game::GenerateTestPattern_ActiveDimming(ID2D1DeviceContext2 * ctx)	//********************** 5.1
{
	D2D1_RECT_F logSize = m_deviceResources->GetLogicalSize();
	ComPtr<ID2D1SolidColorBrush> whiteBrush;			// brush for the "white" color

	float nits = m_outputDesc.MaxLuminance;             // for metadata
	float avg = nits * 0.50f;							// half of peak
	if (m_newTestSelected) SetMetadata(nits, avg, GAMUT_Native);

	float HDR10 = m_activeDimming50PQValue;
	nits = Remove2084( HDR10/1023.0f) * 10000.0f;		// "white" checker brightness

	float color = nitstoCCCS(nits)/BRIGHTNESS_SLIDER_FACTOR;
	switch (m_checkerboard)
	{
	case Checkerboard::Cb6x4:
		DrawChecker6x4(ctx, color);
		break;

	case Checkerboard::Cb4x3:
		DrawChecker4x3(ctx, color);
		break;

	case Checkerboard::Cb4x3not:
		DrawChecker4x3n(ctx, color);
		break;
	}

	// Everything below this point should be hidden during actual measurements.
	if (m_showExplanatoryText)
	{
		std::wstringstream title;
		title << fixed << setw(8) << setprecision(2);

		title << L"5.1 Active Dimming\nMeasure black vs white\n";
		title << L"Nits: ";
		title << nits;
		title << L"  HDR10: ";
		title << setprecision(0);
		title << HDR10;
		title << L" -adjust using up/down arrows\n";
		title << L"Select checkerboard using LessThan < or GreaterThan > keys\n";
		title << L"\n" << m_hideTextString;

		RenderText(ctx, m_largeFormat.Get(), title.str(), m_testTitleRect);

		PrintMetadata(ctx);
	}
	m_newTestSelected = false;

}

void Game::GenerateTestPattern_ActiveDimmingDark(ID2D1DeviceContext2 * ctx) //****************** 5.2
{
	D2D1_RECT_F logSize = m_deviceResources->GetLogicalSize();
	ComPtr<ID2D1SolidColorBrush> whiteBrush;			// brush for the "white" color

	float nits = m_outputDesc.MaxLuminance;             // for metadata
	float avg = nits * 0.50f;							// half of peak
	if (m_newTestSelected) SetMetadata(nits, avg, GAMUT_Native);

	float HDR10 = m_activeDimming05PQValue;
	nits = Remove2084(HDR10 / 1023.0f) * 10000.0f;

	// draw checkerboard of that brightness
	float color = nitstoCCCS(nits)/BRIGHTNESS_SLIDER_FACTOR;
	switch (m_checkerboard)
	{
	case Checkerboard::Cb6x4:
		DrawChecker6x4(ctx, color);
		break;

	case Checkerboard::Cb4x3:
		DrawChecker4x3(ctx, color);
		break;

	case Checkerboard::Cb4x3not:
		DrawChecker4x3n(ctx, color);
		break;
	}


	// Everything below this point should be hidden during actual measurements.
	if (m_showExplanatoryText)
	{
		std::wstringstream title;
		title << fixed << setw(8) << setprecision(3);

		title << L"5.2 Active Dimming Dark\nMeasure black vs white\n";
		title << L"Nits: ";
		title << nits;
		title << L"  HDR10: ";
		title << setprecision(0);
		title << HDR10;
		title << L" -adjust using up/down arrows\n";
		title << L"Select checkerboard using LessThan < or GreaterThan > keys\n";
		title << L"\n" << m_hideTextString;

		RenderText(ctx, m_largeFormat.Get(), title.str(), m_testTitleRect);

		PrintMetadata(ctx);
	}
	m_newTestSelected = false;

}

void Game::GenerateTestPattern_ActiveDimmingSplit(ID2D1DeviceContext2 * ctx) //***************** 5.3
{
	D2D1_RECT_F logSize = m_deviceResources->GetLogicalSize();
	ComPtr<ID2D1SolidColorBrush> whiteBrush;			// brush for the "white" color

	float nits = m_outputDesc.MaxLuminance;             // for metadata
	float avg = nits * 0.50f;							// half of peak
	if (m_newTestSelected) SetMetadata(nits, avg, GAMUT_Native);

	nits = 50.0f;										// "white" checker brightness
	float HDR10 = Apply2084(nits / 10000.f) * 1023.f;	// PQ code
	float colorL = nitstoCCCS(nits)/BRIGHTNESS_SLIDER_FACTOR;

	nits = 5.0f;										// less "white" checker brightness
	HDR10 = Apply2084(nits / 10000.f) * 1023.f;			// PQ Code
	float colorR = nitstoCCCS(nits)/BRIGHTNESS_SLIDER_FACTOR;

	// draw the squares
	switch (m_checkerboard)
	{
	case Checkerboard::Cb6x4:
		DrawChecker6x4(ctx, colorL, colorR);
		break;

	case Checkerboard::Cb4x3:
		DrawChecker4x3(ctx, colorL, colorR);
		break;

	case Checkerboard::Cb4x3not:
		DrawChecker4x3n(ctx, colorL, colorR);
		break;
	}

	// Everything below this point should be hidden during actual measurements.
	if (m_showExplanatoryText)
	{
		nits = 50.0f;										// either 50 or 5 should be displayed
		std::wstringstream title;
		title << fixed << setw(8) << setprecision(2);

		title << L"5.3 Active Dimming Splitscreen\nMeasure black vs white\n";
		title << L"Nits: ";
		title << nits;
		title << L"  HDR10: ";
		title << setprecision(0);
		title << HDR10;
		title << L"\n";
		title << L"Select checkerboard using LessThan < or GreaterThan > keys\n";
		title << L"\n" << m_hideTextString;
 
		RenderText(ctx, m_largeFormat.Get(), title.str(), m_testTitleRect);

		PrintMetadata(ctx);
	}
	m_newTestSelected = false;

}

#define NUMBOXES 32
void Game::GenerateTestPattern_BlackLevelSdrTunnel(ID2D1DeviceContext2 * ctx) //**************** 5. Original v1.0 Tunnel Test
{
	D2D1_RECT_F logSize = m_deviceResources->GetLogicalSize();
	float fBox, fBoxMin, fBoxMax, fBoxDelta;            // box dimension
	fBoxMin = sqrt(4.0f / 100.0f);                      // 4% screen area of center box
	fBoxMax = 1.f;
	fBoxDelta = (fBoxMax - fBoxMin) / NUMBOXES;         // increment for each step

	float nits = 96.0f;                                 // outer brightness
	float avg = 26.0f;
	if (m_newTestSelected) SetMetadata(nits, avg, GAMUT_Native); // average was computed by hand
	float codeMax = 515;                                // outer border set at 96 nits

	ComPtr<ID2D1SolidColorBrush> brushBox;              // brush for the current color
	float codeDelta;
	codeDelta = codeMax / NUMBOXES;

    // standard pattern is 32 nested boxes
    for (int i = 0; i < NUMBOXES; i++)
    {
        fBox = fBoxMax - fBoxDelta*i;
        float code = codeMax - codeDelta*i;
        float3 vCode = float3(code / 1023.f, code / 1023.f, code / 1023.f);
        float3 C = HDR10ToLinear709(vCode);			  // aka c. Represented in CCCS

        if (i >= (NUMBOXES - 1))
            C = 0.0; // make sure center is black

        D2D1_RECT_F centerRect =
        {
            (logSize.right - logSize.left) * (0.5f - fBox*0.5f),
            (logSize.bottom - logSize.top) * (0.5f - fBox*0.5f),
            (logSize.right - logSize.left) * (0.5f + fBox*0.5f),
            (logSize.bottom - logSize.top) * (0.5f + fBox*0.5f)
        };

        DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF(C.x, C.y, C.z), &brushBox));
        ctx->FillRectangle(&centerRect, brushBox.Get());
    }

    // Everything below this point should be hidden during actual measurements.
    if (m_showExplanatoryText)
    {

        // draw the white outline where the sensor goes
        fBox = sqrt(4.0f / 100.0f);
        D2D1_RECT_F centerRect =
        {
            (logSize.right - logSize.left) * (0.5f - fBox*0.5f),
            (logSize.bottom - logSize.top) * (0.5f - fBox*0.5f),
            (logSize.right - logSize.left) * (0.5f + fBox*0.5f),
            (logSize.bottom - logSize.top) * (0.5f + fBox*0.5f)
        };
        ctx->DrawRectangle(centerRect, m_whiteBrush.Get());

        std::wstringstream title;
		title << fixed << setw(8) << setprecision(2);

        title << L"5. Tunnel for SDR black level\nMeasure in the center box";
        title << L"\nNits: ";
        title << nits;
        title << L"  HDR10: ";
		title << setprecision(0);
        title << codeMax;
        title << L"\n" << m_hideTextString;

        RenderText(ctx, m_largeFormat.Get(), title.str(), m_testTitleRect, true);

		PrintMetadata(ctx, true);
	}
    m_newTestSelected = false;

}

void Game::GenerateTestPattern_BlackLevelSdrTunnelTrueBlack(ID2D1DeviceContext2 * ctx) //******** 5. Legacy
{
	D2D1_RECT_F logSize = m_deviceResources->GetLogicalSize();
	float fBox, fBoxMin, fBoxMax, fBoxDelta;            // box dimension
	fBoxMin = sqrt(4.0f / 100.0f);                      // 4% screen area of center box
	fBoxMax = 1.f;
	fBoxDelta = (fBoxMax - fBoxMin) / NUMBOXES;         // increment for each step

	float nits = 80.0f;                                 // outer brightness
	float avg = 68.0f;
	if (m_newTestSelected) SetMetadata(nits, avg, GAMUT_Native); // average was computed by hand

	ComPtr<ID2D1SolidColorBrush> brushBox;              // brush for the current color
	UINT codeSet[14] = { 16, 32, 48, 64, 80, 97, 113, 129, 145, 161, 177, 193, 209, 225 };
	float codeMax = 497;                                // outer border set at 96 nits
	float code;

	// standard pattern is 32 nested boxes
	for (int i = 0; i < NUMBOXES; i++)
	{
		int ri = NUMBOXES - i - 1;				// reverse order
		fBox = fBoxMax - fBoxDelta * i;
		if (ri < 14)
			code = (float) codeSet[ri];
		else
			code = codeMax;

		float3 vCode = float3(code / 1023.f, code / 1023.f, code / 1023.f);
		float3 C = HDR10ToLinear709(vCode);			  // aka c. Represented in CCCS

		if (i >= (NUMBOXES - 1))
			C = 0.0; // make sure center is black

		D2D1_RECT_F centerRect =
		{
			(logSize.right - logSize.left) * (0.5f - fBox * 0.5f),
			(logSize.bottom - logSize.top) * (0.5f - fBox * 0.5f),
			(logSize.right - logSize.left) * (0.5f + fBox * 0.5f),
			(logSize.bottom - logSize.top) * (0.5f + fBox * 0.5f)
		};

		DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF(C.x, C.y, C.z), &brushBox));
		ctx->FillRectangle(&centerRect, brushBox.Get());
	}

	// Everything below this point should be hidden during actual measurements.
	if (m_showExplanatoryText)
	{

		// draw the red outline where the sensor goes
		fBox = sqrt(4.0f / 100.0f);
		D2D1_RECT_F centerRect =
		{
			(logSize.right - logSize.left) * (0.5f - fBox * 0.5f),
			(logSize.bottom - logSize.top) * (0.5f - fBox * 0.5f),
			(logSize.right - logSize.left) * (0.5f + fBox * 0.5f),
			(logSize.bottom - logSize.top) * (0.5f + fBox * 0.5f)
		};
		ctx->DrawRectangle(centerRect, m_redBrush.Get());

		D2D1_ELLIPSE ellipse =
		{
			D2D1::Point2F(100.f, 100.f),
			75.f,
			50.f
		};

		ctx->DrawEllipse(&ellipse, m_redBrush.Get() );

		std::wstringstream title;
		title << fixed << setw(8) << setprecision(2);

		title << L"5. Tunnel for SDR black level\nMeasure in the center box";
		title << L"\nNits: ";
		title << nits;
		title << L"  HDR10: ";
		title << setprecision(0);
		title << codeMax;
		title << L"\n" << m_hideTextString;

		RenderText(ctx, m_largeFormat.Get(), title.str(), m_testTitleRect, true);

		PrintMetadata(ctx, true);
	}
	m_newTestSelected = false;

}

/*
std::wstring colorString(float3 color)
{
	std:wstring str[512];

	wsprintf( str, );
	float3 code = Linear709ToHDR10(code)*1023.0f;			// convert to HDR10
	int wHDR10r = (int)roundf(code.r);
	int wHDR10g = (int)roundf(code.g);
	int wHDR10b = (int)roundf(code.b);

	return str;
}
*/

float3 roundf3(float3 in)
{
	float3 out;
	if (in.x < 0) in.x = 0;
	if (in.y < 0) in.y = 0;
	if (in.z < 0) in.z = 0;

	out.x = roundf(in.x);
	out.y = roundf(in.y);
	out.z = roundf(in.z);
	return out;
}

// Test Pattern 6.
void Game::GenerateTestPattern_ColorPatches			//******************************************* 6.
(
	ID2D1DeviceContext2 * ctx,
	bool fullscreen									// fullscreen vs std patch size
)
{
	float nits;										// Equivalent brightness (for white)
	nits = m_outputDesc.MaxLuminance;				// for 10% OPR case

	float OPR = PATCHPCT;							// On-Pixel-Ratio: Proportion of screen that is test patch
	bool blackText = false;
	if (fullscreen)
	{
		OPR = 1.0f;
		blackText = true;
	}

	if (m_newTestSelected)
	{
		SetMetadata(nits, nits*OPR, GAMUT_Native);	// max and average
	}
    ComPtr<ID2D1SolidColorBrush> redBrush, greenBrush, blueBrush;
    ComPtr<ID2D1SolidColorBrush> blackBrush, whiteBrush;

    // convert EDID primaries from chromaticity coords into sRGB colors for rendering in CCCS
    float2 red_xy, grn_xy, blu_xy, wht_xy;

// This should be close to D6500White (0.31271, 0.32902) as assumed by Windows in most cases
	wht_xy = m_outputDesc.WhitePoint;				// from EDID or INF
	red_xy = m_outputDesc.RedPrimary;
    grn_xy = m_outputDesc.GreenPrimary;
    blu_xy = m_outputDesc.BluePrimary;

#if 0
	// Test 709 primaries
	nits = 270.f;
	red_xy = primaryR_709;
    grn_xy = primaryG_709;
    blu_xy = primaryB_709;

	// Jay's test case
	wht_xy = float2(0.313000, 0.329607); 
    red_xy = float2(0.708508, 0.292492);
    grn_xy = float2(0.170422, 0.797375);
    blu_xy = float2(0.131359, 0.046398);

	// Jean Huang's test case
	nits = 1000.f;
	red_xy = float2(0.681, 0.315);
	grn_xy = float2(0.253, 0.719);
	blu_xy = float2(0.168, 0.048);
	wht_xy = float2(0.319, 0.356);

	// 2020 test case
	nits = 1015.0f;
	wht_xy = D6500White;		// (0.31271, 0.32902)		// Assumed by Windows in most cases
	red_xy = primaryR_2020;
	grn_xy = primaryG_2020;
	blu_xy = primaryB_2020;

	red_xy = primaryR_DCIP3;
	grn_xy = primaryG_DCIP3;
	blu_xy = primaryB_DCIP3;
#endif

	const float WhiteLevel = 1.0f;				// reference value only
    float3 WhiteCol = xytoXYZ(wht_xy, WhiteLevel);
    float3 RedCol   = xytoXYZ(red_xy, WhiteLevel);
    float3 GreenCol = xytoXYZ(grn_xy, WhiteLevel);
    float3 BlueCol  = xytoXYZ(blu_xy, WhiteLevel);

#if 1
	float3x3 PanelMatrix = float3x3(
		RedCol.x, GreenCol.x, BlueCol.x,
		RedCol.y, GreenCol.y, BlueCol.y,
		RedCol.z, GreenCol.z, BlueCol.z
	);
#else
	float3x3 PanelMatrix = float3x3(
		RedCol.x,   RedCol.y,   RedCol.z,
		GreenCol.x, GreenCol.y, GreenCol.z,
		BlueCol.x,  BlueCol.y,  BlueCol.z
	);
#endif
//	float3x3 matTemp = PanelMatrix;		// this produces equivalent results
//	PanelMatrix = transpose(matTemp);

//	float K = 1.6f*nits / 10000.f; 
	float K = nits / 10000.f;
	float3x3 invPanelMatrix = inv(PanelMatrix);
	float3 Yrow = invPanelMatrix*WhiteCol*K;
	 
    float3 RXYZ;
    RXYZ.y = Yrow.x;
    RXYZ.x = RXYZ.y*red_xy.x / red_xy.y;
    RXYZ.z = RXYZ.y*(1.0f - red_xy.x - red_xy.y) / red_xy.y;

    float3 GXYZ;
    GXYZ.y = Yrow.y;
    GXYZ.x = GXYZ.y*grn_xy.x / grn_xy.y;
    GXYZ.z = GXYZ.y*(1.0f - grn_xy.x - grn_xy.y) / grn_xy.y;

    float3 BXYZ;
    BXYZ.y = Yrow.z;
    BXYZ.x = BXYZ.y*blu_xy.x / blu_xy.y;
    BXYZ.z = BXYZ.y*(1.0f - blu_xy.x - blu_xy.y) / blu_xy.y;

//	wht_xy = D6500White;
	float3 WXYZ;
	WXYZ.y = Yrow.w;
	WXYZ.x = WXYZ.y*wht_xy.x / wht_xy.y;
	WXYZ.z = WXYZ.y*(1.0f - wht_xy.x - wht_xy.y) / wht_xy.y;

	// convert to 2020 gamut space	
	float3 R2020 = XYZ_to_BT2020RGB*RXYZ; 
    float3 G2020 = XYZ_to_BT2020RGB*GXYZ;
    float3 B2020 = XYZ_to_BT2020RGB*BXYZ;
    float3 W2020 = XYZ_to_BT2020RGB*WhiteCol*K;
//		   W2020 = XYZ_to_BT2020RGB*WXYZ;

	// apply PQ curve
    float3 RHDR10 = Apply2084(R2020);   
    float3 GHDR10 = Apply2084(G2020);
    float3 BHDR10 = Apply2084(B2020);
	float3 WHDR10 = Apply2084(W2020);

	// convert HDR10 to linear+709 for CCCS rendering
#if 0
	float3x3 tXYZ_to_709RGB = transpose( XYZ_to_709RGB );
    float3 RCCCS = tXYZ_to_709RGB*RXYZ*80.f;
    float3 GCCCS = tXYZ_to_709RGB*GXYZ*80.f;
    float3 BCCCS = tXYZ_to_709RGB*BXYZ*80.f;
	float3 WCCCS = tXYZ_to_709RGB*WhiteCol*K*80.f;
#else
    float3 RCCCS = HDR10ToLinear709(RHDR10);
    float3 GCCCS = HDR10ToLinear709(GHDR10);
    float3 BCCCS = HDR10ToLinear709(BHDR10);
    float3 WCCCS = HDR10ToLinear709(WHDR10);
#endif

    // create D2D brushes for each primary
	DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF(RCCCS.r, RCCCS.g, RCCCS.b), &redBrush));
	DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF(GCCCS.r, GCCCS.g, GCCCS.b), &greenBrush));
	DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF(BCCCS.r, BCCCS.g, BCCCS.b), &blueBrush));
	DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF(WCCCS.r, WCCCS.g, WCCCS.b), &whiteBrush));

    std::wstringstream title;
	title << fixed << setw(8) << setprecision(2);

    title << L"6. Checking ";	// show test number

	// convert to 10-bit code values for printout display
	RHDR10 = Apply2084(R2020*BRIGHTNESS_SLIDER_FACTOR)*1023.f;           // apply PQ curve
	GHDR10 = Apply2084(G2020*BRIGHTNESS_SLIDER_FACTOR)*1023.f;
	BHDR10 = Apply2084(B2020*BRIGHTNESS_SLIDER_FACTOR)*1023.f;
	WHDR10 = Apply2084(W2020*BRIGHTNESS_SLIDER_FACTOR)*1023.f;

	float fSize = sqrt(OPR);
	float dpi = m_deviceResources->GetDpi();
	D2D1_RECT_F logSize = m_deviceResources->GetLogicalSize();

	D2D1_RECT_F centerRect;
	if (fullscreen)
		centerRect = logSize;
	else
	{
		float patchPct = OPR;			// patch percentage of screen area
		float size = sqrt((logSize.right - logSize.left) * (logSize.bottom - logSize.top));
		size = size * sqrtf(OPR);		// dimensions for a square of this % screen area

		float2 center;
		center.x = (logSize.right - logSize.left) * 0.50f;
		center.y = (logSize.bottom - logSize.top) * 0.50f;

		float2 jitter;
		float radius = JITTER_RADIUS * dpi / 96.0f;
		do {
			jitter.x = radius * (randf_s() * 2.f - 1.f);
			jitter.y = radius * (randf_s() * 2.f - 1.f);
		} while ((jitter.x * jitter.x + jitter.y * jitter.y) > radius);

		if (fullscreen) jitter = float2(0.f, 0.f);

		// Apply jitter
		center = center + jitter;

		centerRect =
		{
			center.x - size * 0.50f,
			center.y - size * 0.50f,
			center.x + size * 0.50f,
			center.y + size * 0.50f
		};
	}
  
    switch (m_currentColor)
    {
    case 0:
        ctx->FillRectangle(centerRect, redBrush.Get());
        title << L"Red Chromaticity Point\n xy:    ";
        title << setprecision(5) << red_xy.x << ", " << red_xy.y << "\n";
		title << L"CCCS:  ";
		title << setprecision(3) << RCCCS.r << ", " << RCCCS.g << ", " << RCCCS.b << "\n";
		title << L"HDR10: ";
        title << setprecision(0) << RHDR10.r << ", " << RHDR10.g << ", " << RHDR10.b;
        break;

    case 1:
        ctx->FillRectangle(centerRect, greenBrush.Get());
        title << L"Green Chromaticity Point\n xy:    ";
        title << setprecision(5) << grn_xy.x << ", " << grn_xy.y << "\n";
		title << L"CCCS:  ";
		title << setprecision(3) << GCCCS.r << ", " << GCCCS.g << ", " << GCCCS.b << "\n";
		title << L"HDR10: ";
		title << setprecision(0) << GHDR10.r << ", " << GHDR10.g << ", " << GHDR10.b;
        break;

    case 2:
        ctx->FillRectangle(centerRect, blueBrush.Get());
        title << L"Blue Chromaticity Point\n xy:    ";
        title << setprecision(5) << blu_xy.x << ", " << blu_xy.y << "\n";
		title << L"CCCS:  ";
		title << setprecision(3) << BCCCS.r << ", " << BCCCS.g << ", " << BCCCS.b << "\n";
		title << L"HDR10: ";
		title << setprecision(0) << BHDR10.r << ", " << BHDR10.g << ", " << BHDR10.b;
        break;

    case 3:
        ctx->FillRectangle(centerRect, whiteBrush.Get());
        title << L"White Point\n xy:    ";
		title << setprecision(5) << wht_xy.x << ", " << wht_xy.y << "\n";
        title << L"CCCS:  ";
		title << setprecision(3) << WCCCS.r << ", " << WCCCS.g << ", " << WCCCS.b << "\n";
        //		title << L"\nNits:    ";
        //		title << std::to_wstring(nits) << ", " << std::to_wstring(nits) << ", " << std::to_wstring(nits);
        title << L"HDR10: ";
        title << setprecision(0) << WHDR10.r << ", " << WHDR10.g << ", " << WHDR10.b;
        break;
    }

    if (m_showExplanatoryText)
    {
		// Draw outline/borders to track clipped limit (like old v1.0 color 6.B test)
		K = m_outputDesc.MaxLuminance / 10000.f;
		invPanelMatrix = inv(XYZ_to_BT2020RGB);		// assume 2020 primaries for reference and hope they get clipped to actual
		Yrow = invPanelMatrix * WhiteCol * K;
//		Yrow = XYZ_to_BT2020RGB * WhiteCol * K;

		RXYZ.y = Yrow.x;
		RXYZ.x = RXYZ.y * red_xy.x / red_xy.y;
		RXYZ.z = RXYZ.y * (1.0f - red_xy.x - red_xy.y) / red_xy.y;

		GXYZ.y = Yrow.y;
		GXYZ.x = GXYZ.y * grn_xy.x / grn_xy.y;
		GXYZ.z = GXYZ.y * (1.0f - grn_xy.x - grn_xy.y) / grn_xy.y;

		BXYZ.y = Yrow.z;
		BXYZ.x = BXYZ.y * blu_xy.x / blu_xy.y;
		BXYZ.z = BXYZ.y * (1.0f - blu_xy.x - blu_xy.y) / blu_xy.y;

		WXYZ.y = Yrow.w;
		WXYZ.x = WXYZ.y * wht_xy.x / wht_xy.y;
		WXYZ.z = WXYZ.y * (1.0f - wht_xy.x - wht_xy.y) / wht_xy.y;

		// convert to 2020 gamut space	
		R2020 = XYZ_to_BT2020RGB * RXYZ;
		G2020 = XYZ_to_BT2020RGB * GXYZ;
		B2020 = XYZ_to_BT2020RGB * BXYZ;
		W2020 = XYZ_to_BT2020RGB * WhiteCol * K;
		// W2020 = XYZ_to_BT2020RGB*WXYZ;

		// apply PQ curve
		RHDR10 = Apply2084(R2020);
		GHDR10 = Apply2084(G2020);
		BHDR10 = Apply2084(B2020);
		WHDR10 = Apply2084(W2020);

		// convert HDR10 to linear+709 for CCCS rendering
#if 0
		float3x3 tXYZ_to_709RGB = transpose(XYZ_to_709RGB);
		float3 RCCCS = tXYZ_to_709RGB * RXYZ * 80.f;
		float3 GCCCS = tXYZ_to_709RGB * GXYZ * 80.f;
		float3 BCCCS = tXYZ_to_709RGB * BXYZ * 80.f;
		float3 WCCCS = tXYZ_to_709RGB * WhiteCol * K * 80.f;
#else
		float3 RCCCS = HDR10ToLinear709(RHDR10);
		float3 GCCCS = HDR10ToLinear709(GHDR10);
		float3 BCCCS = HDR10ToLinear709(BHDR10);
		float3 WCCCS = HDR10ToLinear709(WHDR10);
#endif

		BCCCS.r = 0.f;

		DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF(RCCCS.r, RCCCS.g, RCCCS.b), &redBrush));
		DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF(GCCCS.r, GCCCS.g, GCCCS.b), &greenBrush));
		DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF(BCCCS.r, BCCCS.g, BCCCS.b), &blueBrush));
		DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF(WCCCS.r, WCCCS.g, WCCCS.b), &whiteBrush));

		// draw the rect in the correct color
		switch (m_currentColor)
		{
		case 0:
			ctx->DrawRectangle(centerRect, redBrush.Get(),   12);	break;
		case 1:
			ctx->DrawRectangle(centerRect, greenBrush.Get(), 12);	break;
		case 2:
			ctx->DrawRectangle(centerRect, blueBrush.Get(),  12);	break;
		case 3:
			ctx->DrawRectangle(centerRect, whiteBrush.Get(), 12);	break;
		}

		title << L"\nUp & Down arrow keys rotate between RGBW colors\n";

		RenderText(ctx, m_largeFormat.Get(), title.str(), m_testTitleRect );

		PrintMetadata(ctx);

	}
    m_newTestSelected = false;

}

void Game::GenerateTestPattern_ColorPatches709(ID2D1DeviceContext2 * ctx)
{
    if (m_newTestSelected) SetMetadataNeutral();    // This simulates desktop content where default metadata is used.

    // Overload the color selector to allow selecting the desired SDR white level.
    float nits = 80.0f;
    switch (m_currentColor)
    {
    case 0:
        nits = 80.0f;
        break;
    case 1:
        nits = 160.0f;
        break;
    case 2:
        nits = 240.0f;
        break;
    case 3:
        nits = 320.0f;
        break;
    default:
        DX::ThrowIfFailed(E_INVALIDARG);
        break;
    }

    ComPtr<ID2D1SolidColorBrush> redBrush, greenBrush, blueBrush;

    // Generating the colors is trivial since 709 primaries == CCCS primaries and we are emulating
    // SDR boost which simply scales RGB colors linearly.
    DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF(nitstoCCCS(nits), 0.0f, 0.0f), &redBrush));
    DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF(0.0f, nitstoCCCS(nits), 0.0f), &greenBrush));
    DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.0f, nitstoCCCS(nits)), &blueBrush));

    auto full = m_deviceResources->GetLogicalSize();
    // Divide screen into thirds.
    // D2D1_RECT_F: Left, Top, Right, Bottom
    D2D1_RECT_F redRect   = { full.left, (full.bottom - full.top) * 0.0f / 3.0f , full.right, (full.bottom - full.top) * 1.0f / 3.0f };
    D2D1_RECT_F greenRect = { full.left, (full.bottom - full.top) * 1.0f / 3.0f , full.right, (full.bottom - full.top) * 2.0f / 3.0f };
    D2D1_RECT_F blueRect  = { full.left, (full.bottom - full.top) * 2.0f / 3.0f , full.right, (full.bottom - full.top) * 3.0f / 3.0f };

    ctx->FillRectangle(redRect, redBrush.Get());
    ctx->FillRectangle(greenRect, greenBrush.Get());
    ctx->FillRectangle(blueRect, blueBrush.Get());

    if (m_showExplanatoryText)
    {
        std::wstringstream title;
		title << fixed << setw(8) << setprecision(2);

        title << L"709 primaries\nUsing reference level of: " << nits;
        title << " nits.\nUp & Down arrow keys select white level.\n";

        RenderText(ctx, m_largeFormat.Get(), title.str(), m_testTitleRect);

		PrintMetadata(ctx);
	}

    m_newTestSelected = false;
}

// aka 6.b from v1.0
void Game::GenerateTestPattern_ColorPatchesMAX(ID2D1DeviceContext2 * ctx, float OPR) // *******6.MAX
{
    ComPtr<ID2D1SolidColorBrush> redBrush, greenBrush, blueBrush;
    ComPtr<ID2D1SolidColorBrush> blackBrush, whiteBrush;

    // convert EDID primaries from chromaticity coords into sRGB colors for rendering in CCCS
    float2 red_xy, grn_xy, blu_xy, wht_xy;

    wht_xy = D6500White;
    red_xy = primaryR_2020;
    grn_xy = primaryG_2020;
    blu_xy = primaryB_2020;

    // convert primary colors to RGB709 space
    float3 R = xytoXYZ(red_xy, 1.0)*XYZ_to_709RGB;
    float3 G = xytoXYZ(grn_xy, 1.0)*XYZ_to_709RGB;
    float3 B = xytoXYZ(blu_xy, 1.0)*XYZ_to_709RGB;
    float3 W = xytoXYZ(wht_xy, 1.0)*XYZ_to_709RGB;
    //	float3 W = float3(Y, Y, Y);

    // Adjust them to desired output brightness level
 // const float nits = 300.0f;								// white level should be 300 cd/m2
	const float nits = m_outputDesc.MaxLuminance;			// set to value from EDID 10% peak
    if (m_newTestSelected) SetMetadata(nits, nits*OPR, GAMUT_BT2100); // max and average are same for full screen

    const float Y = nitstoCCCS(nits);
    float3 YRow = float3(0.069095, 0.919544, 0.011360);
    R = R*Y;
    G = G*Y;
    B = B*Y;
    W = W*Y;

    float3 Rspec = float3(636. / 1023., 0., 0.);
    float3 Gspec = float3(0., 636. / 1023., 0.);
    float3 Bspec = float3(0., 0., 636. / 1023.);
    float3 Wspec = float3(636. / 1023., 636. / 1023., 636. / 1023.);

    R = HDR10ToLinear709(Rspec);
    G = HDR10ToLinear709(Gspec);
    B = HDR10ToLinear709(Bspec);
    W = HDR10ToLinear709(Wspec);

    // create D2D brushes for each primary
    DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF(R.r, R.g, R.b), &redBrush));
    DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF(G.r, G.g, G.b), &greenBrush));
    DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF(B.r, B.g, B.b), &blueBrush));

    DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF(0.0, 0.0, 0.0), &blackBrush));
    DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF(W.r, W.g, W.b), &whiteBrush));

    std::wstringstream title;
	title << fixed << setw(8) << setprecision(2);

    title << L"6.b Checking ";	// show test number

	float fSize = sqrt(OPR);
	D2D1_RECT_F logSize = m_deviceResources->GetLogicalSize();
	D2D1_RECT_F centerRect =
	{
		(logSize.right - logSize.left) * (0.5f - fSize * 0.5f),
		(logSize.bottom - logSize.top) * (0.5f - fSize * 0.5f),
		(logSize.right - logSize.left) * (0.5f + fSize * 0.5f),
		(logSize.bottom - logSize.top) * (0.5f + fSize * 0.5f)
	};

    float3 HDR10;
    switch (m_currentColor)
    {
    case 0:
        ctx->FillRectangle(centerRect, redBrush.Get());
        title << L"Red Chromaticity Point\n xy: ";
        title << std::to_wstring(red_xy.x) << ", " << std::to_wstring(red_xy.y) << "\n";
        title << std::to_wstring(R.r) << ", " << std::to_wstring(R.g) << ", " << std::to_wstring(R.b) << "\n";
        HDR10 = Linear709ToHDR10(R)*1023.0f;						// convert to HDR10
        HDR10 = roundf3(HDR10);
        title << std::to_wstring((int)HDR10.r) << ", " << std::to_wstring((int)HDR10.g) << ", " << std::to_wstring((int)HDR10.b);
        break;

    case 1:
        ctx->FillRectangle(centerRect, greenBrush.Get());
        title << L"Green Chromaticity Point\n xy: ";
        title << std::to_wstring(grn_xy.x) << ", " << std::to_wstring(grn_xy.y) << "\n";
        title << std::to_wstring(G.r) << ", " << std::to_wstring(G.g) << ", " << std::to_wstring(G.b) << "\n";
        HDR10 = Linear709ToHDR10(G)*1023.0f;						// convert to HDR10
        HDR10 = roundf3(HDR10);
        title << std::to_wstring((int)HDR10.r) << ", " << std::to_wstring((int)HDR10.g) << ", " << std::to_wstring((int)HDR10.b);
        break;

    case 2:
        ctx->FillRectangle(centerRect, blueBrush.Get());
        title << L"Blue Chromaticity Point\n xy: ";
        title << std::to_wstring(blu_xy.x) << ", " << std::to_wstring(blu_xy.y) << "\n";
        title << std::to_wstring(B.r) << ", " << std::to_wstring(B.g) << ", " << std::to_wstring(B.b) << "\n";
        HDR10 = Linear709ToHDR10(B)*1023.0f;						// convert to HDR10
        HDR10 = roundf3(HDR10);
        title << std::to_wstring((int)HDR10.r) << ", " << std::to_wstring((int)HDR10.g) << ", " << std::to_wstring((int)HDR10.b);
        break;

    case 3:
        ctx->FillRectangle(centerRect, whiteBrush.Get());
        title << L"White Point\n xy: ";
        title << std::to_wstring(wht_xy.x) << ", " << std::to_wstring(wht_xy.y);
        title << L"\nCCCS:    ";
        title << std::to_wstring(W.r) << ", " << std::to_wstring(W.g) << ", " << std::to_wstring(W.b);
        //		title << L"\nNits:    ";
        //		title << std::to_wstring(nits) << ", " << std::to_wstring(nits) << ", " << std::to_wstring(nits);
        title << L"\nHDR10: ";
        HDR10 = Linear709ToHDR10(W)*1023.0f;						// convert to HDR10
        HDR10 = roundf3(HDR10);
        title << std::to_wstring((int)HDR10.r) << ", " << std::to_wstring((int)HDR10.g) << ", " << std::to_wstring((int)HDR10.b);
        break;
    }

    if (m_showExplanatoryText)
    {
        title << L"\nUp & Down arrow keys rotate between RGBW colors\n";

        RenderText(ctx, m_largeFormat.Get(), title.str(), m_testTitleRect );

		PrintMetadata(ctx);
	}
    m_newTestSelected = false;

}


#if 0
void Game::GenerateTestPattern_ColorPatches(ID2D1DeviceContext2 * ctx)
{
    // Create colors for 50% sRGB, 100% sRGB, DCI-P3, and Rec. 2020.
    ComPtr<ID2D1SolidColorBrush> red50, redSrgb, redP3, red2020;
    ComPtr<ID2D1SolidColorBrush> green50, greenSrgb, greenP3, green2020;
    ComPtr<ID2D1SolidColorBrush> blue50, blueSrgb, blueP3, blue2020;

    // CCCS values are reverse engineered from FV2 tool test patterns.
    DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF( 0.994f,  0.498f,  0.515f), &red50));
    DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF( 1.000f,  0.000f,  0.000f), &redSrgb));
    DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF( 1.132f, -0.039f, -0.017f), &redP3));
    DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF( 3.172f, -0.234f, -0.033f), &red2020));

    DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF( 0.522f,  0.989f,  0.513f), &green50));
    DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF( 0.000f,  1.000f,  0.000f), &greenSrgb));
    DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF(-0.221f,  1.062f, -0.079f), &greenP3));
    DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF(-0.433f,  0.822f, -0.074f), &green2020));

    DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF( 0.512f,  0.493f,  1.000f), &blue50));
    DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF( 0.000f,  0.000f,  1.000f), &blueSrgb));
    DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF( 0.000f,  0.000f,  1.000f), &blueP3));
    DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF(-0.609f, -0.065f,  9.297f), &blue2020));

    // Divide pattern in a 3x4 grid.
    auto rect = m_deviceResources->GetLogicalSize();
    auto w = (rect.right - rect.left) / 4.0f;
    auto h = (rect.bottom - rect.top) / 3.0f;
    auto w1 = w * 0.2f; // Left/top edge of each patch offset.
    auto h1 = h * 0.2f;
    auto w2 = w * 0.8f; // Right/bottom edge of each patch offset.
    auto h2 = h * 0.8f;

    ctx->FillRectangle(D2D1::RectF(w * 0.0f + w1, h * 0.0f + h1, w * 0.0f + w2, h * 0.0f + h2), red50.Get());
    ctx->FillRectangle(D2D1::RectF(w * 1.0f + w1, h * 0.0f + h1, w * 1.0f + w2, h * 0.0f + h2), redSrgb.Get());
    ctx->FillRectangle(D2D1::RectF(w * 2.0f + w1, h * 0.0f + h1, w * 2.0f + w2, h * 0.0f + h2), redP3.Get());
    // TODO: Unsure if the Rec. 2020 colors are indeed correct. Double check and add them back.
    //ctx->FillRectangle(D2D1::RectF(w * 3.0f + w1, h * 0.0f + h1, w * 3.0f + w2, h * 0.0f + h2), red2020.Get());
    ctx->FillRectangle(D2D1::RectF(w * 0.0f + w1, h * 1.0f + h1, w * 0.0f + w2, h * 1.0f + h2), green50.Get());
    ctx->FillRectangle(D2D1::RectF(w * 1.0f + w1, h * 1.0f + h1, w * 1.0f + w2, h * 1.0f + h2), greenSrgb.Get());
    ctx->FillRectangle(D2D1::RectF(w * 2.0f + w1, h * 1.0f + h1, w * 2.0f + w2, h * 1.0f + h2), greenP3.Get());
    //ctx->FillRectangle(D2D1::RectF(w * 3.0f + w1, h * 1.0f + h1, w * 3.0f + w2, h * 1.0f + h2), green2020.Get());
    ctx->FillRectangle(D2D1::RectF(w * 0.0f + w1, h * 2.0f + h1, w * 0.0f + w2, h * 2.0f + h2), blue50.Get());
    ctx->FillRectangle(D2D1::RectF(w * 1.0f + w1, h * 2.0f + h1, w * 1.0f + w2, h * 2.0f + h2), blueSrgb.Get());
    ctx->FillRectangle(D2D1::RectF(w * 2.0f + w1, h * 2.0f + h1, w * 2.0f + w2, h * 2.0f + h2), blueP3.Get());
    //ctx->FillRectangle(D2D1::RectF(w * 3.0f + w1, h * 2.0f + h1, w * 3.0f + w2, h * 2.0f + h2), blue2020.Get());

    // Everything below this point should be hidden for actual measurements.
    if (!m_showExplanatoryText)
    {
        return;
    }

    RenderText(ctx, m_smallFormat.Get(), L"Red: 50% sRGB   ", { w * 0.0f + w1, h * 1.0f - h1 / 1.0f, 200.0f, 30.0f });
    RenderText(ctx, m_smallFormat.Get(), L"Red: 100% sRGB  ", { w * 1.0f + w1, h * 1.0f - h1 / 1.0f, 200.0f, 30.0f });
    RenderText(ctx, m_smallFormat.Get(), L"Red: DCI-P3     ", { w * 2.0f + w1, h * 1.0f - h1 / 1.0f, 200.0f, 30.0f });
    //RenderText(ctx, m_smallFormat.Get(), L"Red: Rec. 2020  ", { w * 3.0f + w1, h * 1.0f - h1 / 1.0f, 200.0f, 30.0f });
    RenderText(ctx, m_smallFormat.Get(), L"Green: 50% sRGB ", { w * 0.0f + w1, h * 2.0f - h1 / 1.0f, 200.0f, 30.0f });
    RenderText(ctx, m_smallFormat.Get(), L"Green: 100% sRGB", { w * 1.0f + w1, h * 2.0f - h1 / 1.0f, 200.0f, 30.0f });
    RenderText(ctx, m_smallFormat.Get(), L"Green: DCI-P3   ", { w * 2.0f + w1, h * 2.0f - h1 / 1.0f, 200.0f, 30.0f });
    //RenderText(ctx, m_smallFormat.Get(), L"Green: Rec. 2020", { w * 3.0f + w1, h * 2.0f - h1 / 1.0f, 200.0f, 30.0f });
    RenderText(ctx, m_smallFormat.Get(), L"Blue: 50% sRGB  ", { w * 0.0f + w1, h * 3.0f - h1 / 1.0f, 200.0f, 30.0f });
    RenderText(ctx, m_smallFormat.Get(), L"Blue: 100% sRGB ", { w * 1.0f + w1, h * 3.0f - h1 / 1.0f, 200.0f, 30.0f });
    RenderText(ctx, m_smallFormat.Get(), L"Blue: DCI-P3    ", { w * 2.0f + w1, h * 3.0f - h1 / 1.0f, 200.0f, 30.0f });
    //RenderText(ctx, m_smallFormat.Get(), L"Blue: Rec. 2020 ", { w * 3.0f + w1, h * 3.0f - h1 / 1.0f, 200.0f, 30.0f });

    std::wstring text = L"RGB color patches\n" << m_hideTextString;
    RenderText(ctx, m_largeFormat.Get(), text, m_testTitleRect);
}
#endif

void Game::GenerateTestPattern_BitDepthPrecision(ID2D1DeviceContext2 * ctx) //******************* 7.
{
    if (m_newTestSelected)
		SetMetadata( m_outputDesc.MaxLuminance, 2.0f, GAMUT_Native);

    // TODO: merge into common effect test pattern generator.
    auto rsc = m_testPatternResources[TestPattern::BitDepthPrecision];

    std::wstringstream title;
    auto rect = m_deviceResources->GetOutputSize();

    if (!rsc.effectIsValid)
    {
        title << L"\nERROR: BandedGradientEffect.cso is missing\n";
    }
    else
    {
        DX::ThrowIfFailed(rsc.d2dEffect->SetValueByName(
            L"OutputSize",
            D2D1::Point2F((float)rect.right - rect.left, (float)rect.bottom - rect.top)));

        ctx->DrawImage(rsc.d2dEffect.Get());
    }

    // Everything below this point should be hidden for actual measurements.
    if (m_showExplanatoryText)
    {
        title << rsc.testTitle << L"\n" << m_hideTextString;
        RenderText(ctx, m_largeFormat.Get(), title.str(), m_testTitleRect);

        // Text labels are manually aligned with offsets from BandedGradientEffect.hlsl

        D2D1_RECT_F text10Rect = { 0.0f, (rect.bottom - rect.top) * 0.1f, 300.0f, 50.0f };
        D2D1_RECT_F text30Rect = { 0.0f, (rect.bottom - rect.top) * 0.3f, 300.0f, 50.0f };
        D2D1_RECT_F text50Rect = { 0.0f, (rect.bottom - rect.top) * 0.5f, 300.0f, 50.0f };
        D2D1_RECT_F text70Rect = { 0.0f, (rect.bottom - rect.top) * 0.7f, 300.0f, 50.0f };
        D2D1_RECT_F text90Rect = { 0.0f, (rect.bottom - rect.top) * 0.9f, 300.0f, 50.0f };

        RenderText(ctx, m_largeFormat.Get(), L"6-bit quantization", text10Rect);
        RenderText(ctx, m_largeFormat.Get(), L"Display-native", text30Rect);
        //	RenderText(ctx, m_largeFormat.Get(), L"6+2 dither 2x2",		text30Rect);
        RenderText(ctx, m_largeFormat.Get(), L"8-bit quantization", text50Rect);
        RenderText(ctx, m_largeFormat.Get(), L"Display-native", text70Rect);
        RenderText(ctx, m_largeFormat.Get(), L"10-bit quantization", text90Rect);

		PrintMetadata(ctx);
	}

    m_newTestSelected = false;
}

void Game::GenerateTestPattern_RiseFallTime(ID2D1DeviceContext2 * ctx) //************************ 8.
{
//  "tone map" PQ limit of 10k nits down to panel maxLuminance in CCCS
//  float nits = m_outputDesc.MaxLuminance;
//	float avg = nits * 0.1f; // 10% screen area
	float nits = m_outputDesc.MaxLuminance;
	float patchPCT = PATCHPCT;
	float avg = m_outputDesc.MaxLuminance*patchPCT;

	if (m_newTestSelected) SetMetadata(nits, avg, GAMUT_Native);
    
    auto logSize = m_deviceResources->GetLogicalSize();
	float dpi = m_deviceResources->GetDpi();
	std::wstringstream title;

#if 0
	// Draw the background noise effect
	auto rsc = m_testPatternResources[TestPattern::TenPercentPeak];
	if (!rsc.effectIsValid)
	{
		title << L"\nERROR: " << rsc.effectShaderFilename << L" is missing\n";
	}
	else
	{
		auto out = m_deviceResources->GetOutputSize();
		D2D1_POINT_2F time = { m_totalTime, 0.f };

		// init var.
		float init = 30.0f;

		// How many pixels before the wavelength halves.
		float dist = (out.right - out.left) / 4.0f;

		rsc.d2dEffect->SetValueByName(L"Center", time);
		rsc.d2dEffect->SetValueByName(L"InitialWavelength", init);
		rsc.d2dEffect->SetValueByName(L"WavelengthHalvingDistance", dist);
		rsc.d2dEffect->SetValueByName(L"WhiteLevelMultiplier", nitstoCCCS(185));

		ctx->DrawImage(rsc.d2dEffect.Get());
	}

	// draw a starfield background
	float starNits = min(nits, 100.f);							// clamp nits to max of 100
	ComPtr<ID2D1SolidColorBrush> starBrush;
	D2D1_ELLIPSE ellipse;
	float pixels = 0;
	srand(314158);												// seed the starfield rng
	for (int i = 1; i < 2000; i++)
	{
		float s = nitstoCCCS((randf()) * starNits);
		DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF(s, s, s), &starBrush));

		float2 center = float2(logSize.right * randf(), logSize.bottom * randf());
		float fRad = randf() * randf() * randf() * 19.f + 1.f;
		ellipse =
		{
			D2D1::Point2F(center.x, center.y),
			fRad, fRad
		};
		ctx->FillEllipse(&ellipse, starBrush.Get());

		pixels += M_PI_F * fRad * fRad * s;

	}
	float area = (logSize.right - logSize.left) * (logSize.bottom - logSize.top);
	float APL = pixels / area;
#endif

	// draw center square
	float c = nitstoCCCS(nits);
	ComPtr<ID2D1SolidColorBrush> centerBrush;
	DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF(c, c, c), &centerBrush));

	float size = sqrt((logSize.right - logSize.left) * (logSize.bottom - logSize.top));
	float patchPct = PATCHPCT;
	size = size * sqrtf(patchPct);  // dimensions for a square of this % screen area
	float2 center;
	center.x = (logSize.right - logSize.left) * 0.50f;
	center.y = (logSize.bottom - logSize.top) * 0.50f;

	float2 jitter;
	float radius = JITTER_RADIUS * dpi / 96.0f;

	do {
		jitter.x = radius * (randf_s() * 2.f - 1.f);
		jitter.y = radius * (randf_s() * 2.f - 1.f);
	} while ((jitter.x * jitter.x + jitter.y * jitter.y) > radius);

	// Apply jitter
	center = center + jitter;

	D2D1_RECT_F centerRect =
	{
		center.x - size * 0.50f,
		center.y - size * 0.50f,
		center.x + size * 0.50f,
		center.y + size * 0.50f
	};

    if (m_flashOn)
        ctx->FillRectangle(&centerRect, centerBrush.Get());

    if (m_showExplanatoryText)
    {
		title << fixed << setw(8) << setprecision(2);

        title << L"8. Rise/Fall Time";
        title << L"\nNits: ";
        title << nits*BRIGHTNESS_SLIDER_FACTOR;
        title << L"  HDR10: ";
		title << setprecision(0);
        title << Apply2084(c*80.f*BRIGHTNESS_SLIDER_FACTOR / 10000.f) * 1023.f;
        title << L"\n";
        title << setprecision(2) << m_testTimeRemainingSec;
        title << L" seconds remaining";
        title << L"\n" << m_hideTextString;

        RenderText(ctx, m_largeFormat.Get(), title.str(), m_testTitleRect);

		PrintMetadata(ctx);
	}
    m_newTestSelected = false;
}

void Game::GenerateTestPattern_ProfileCurve(ID2D1DeviceContext2 * ctx)  //*********************** 9.
{
#define NPQCODES 45
	uint PQCodes[NPQCODES] =
	{
		1023,
		0,
		8,
		16,
		24,
		36,
		48,
		56,
		64,
		120,
		156,
		256,
		340,
		384,
		452,
		488,
		520,
		592,
		616,
		636,
		660,
		664,
		668,
		688,
		692,
		704,
		708,
		712,
		728,
		744,
		756,
		760,
		764,
		768,
		788,
		804,
		808,
		812,
		828,
		840,
		844,
		872,
		892,
		920,
		1023
	};

	if (m_newTestSelected)
	{
		SetMetadata( m_outputDesc.MaxLuminance, m_outputDesc.MaxLuminance*0.10f, GAMUT_Native );
	}

	// find the brightest tile we need to test on this panel
	for (int i = 3; i < NPQCODES; i++)
	{
		m_maxProfileTile = i;
		if (PQCodes[i] > m_maxPQCode)
			break;
	}

	// get current intensity value to display on tile
	UINT PQCode = PQCodes[m_currentProfileTile];
	if (PQCode > m_maxPQCode) PQCode = m_maxPQCode;				// clamp to max reported possible

	float nits = Remove2084( PQCode / 1023.0f)*10000.0f;		// go to linear space
	float c = nitstoCCCS(nits/BRIGHTNESS_SLIDER_FACTOR);		// scale by 80 and slider

	// "tone map" PQ limit of 10k nits down to panel maxLuminance in CCCS
	float avg = nits * 0.1f;									// 10% screen area
	if (m_newTestSelected) {
		SetMetadata(nits, avg, GAMUT_Native);
	}

	std::wstringstream title;

	// Draw the background noise effect
	auto rsc = m_testPatternResources[TestPattern::TenPercentPeak];
	if (!rsc.effectIsValid)
	{
		title << L"\nERROR: " << rsc.effectShaderFilename << L" is missing\n";
	}
	else
	{
		float TierMaxLum = GetTierLuminance(m_testingTier);
		float noiseAPL = TierMaxLum * 0.02174f;					// assume noise covers 92% of screen
		if (noiseAPL > 100.0f) noiseAPL = 100.0f;				// clamp noise APL to max of 100 nits in all cases
		float limit = 100.0f;									// clamp all noise pixel values to this
		if (limit > nits) limit = nits;							// no noise pixels brighter than patch
		if (noiseAPL > limit) noiseAPL = limit;					// don't have average APL higher than pixel limit

		// ******* Arguments to this call are not type checked  *********
		// ******* DO NOT PASS A DOUBLE HERE (EVEN LITERAL!) *** ELSE FAIL ************* 
		rsc.d2dEffect->SetValueByName(L"APL", noiseAPL );	// ****** MUST BE FLOAT!		// average luminance of noise in nits
		rsc.d2dEffect->SetValueByName(L"Clamp", limit );									// no pixels above this (nits)
		rsc.d2dEffect->SetValueByName(L"iTime", m_totalTime);								// seconds

		ctx->DrawImage(rsc.d2dEffect.Get());
	}

	// draw center square
	float dpi = m_deviceResources->GetDpi();
	auto logSize = m_deviceResources->GetLogicalSize();

	ComPtr<ID2D1SolidColorBrush> centerBrush;
	DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF(c, c, c), &centerBrush));

	float size = sqrt((logSize.right - logSize.left) * (logSize.bottom - logSize.top));
	float patchPct = PATCHPCT;
	size = size * sqrtf(patchPct);  // dimensions for a square of this % screen area
	float2 center;
	center.x = (logSize.right - logSize.left) * 0.50f;
	center.y = (logSize.bottom - logSize.top) * 0.50f;

	float2 jitter;
	float radius = JITTER_RADIUS * dpi / 96.0f;

	do {
		jitter.x = radius * (randf_s() * 2.f - 1.f);
		jitter.y = radius * (randf_s() * 2.f - 1.f);
	} while ((jitter.x * jitter.x + jitter.y * jitter.y) > radius);

	// Apply jitter
	center = center + jitter;

	D2D1_RECT_F centerRect =
	{
		center.x - size * 0.50f,
		center.y - size * 0.50f,
		center.x + size * 0.50f,
		center.y + size * 0.50f
	};
	ctx->FillRectangle(&centerRect, centerBrush.Get());

	float PQcheck = Apply2084(c * 80.f * BRIGHTNESS_SLIDER_FACTOR / 10000.f) * 1023.f;

	if (m_showExplanatoryText)
	{
		// draw target circle
//		float fRad = sqrt((logSize.right - logSize.left) * (logSize.bottom - logSize.top)*0.04f)*0.35;	// 4% screen area colorimeter box
		float fRad = 0.5f * m_snoodDiam / 25.4f * dpi * 1.2f;      // radius of dia 27mm -> inches -> dips
		float2 center = float2(logSize.right * 0.5f, logSize.bottom * 0.5f);

		D2D1_ELLIPSE ellipse =
		{
			D2D1::Point2F(center.x, center.y),
			fRad, fRad
		};
		ctx->DrawEllipse(&ellipse, m_redBrush.Get(), 1 );

		title << fixed << setw(8) << setprecision(0);
		title << L"9. Validating 2084 Profile Curve in nits\n";
		title << L"Subtest#: ";
		title << setw(2) << m_currentProfileTile;
		title << L"   HDR10: ";
		title << setw(3) << PQCode;
		title << L"   Nits: ";
		title << setw(4) << setprecision(0) << nits;
//		title << L"   HDR10b: ";
//		title << setprecision(4) << PQcheck;		// echo input to validate precision
		title << L"\nUp/Down arrow keys select level";
		title << L"\n";
		title << m_hideTextString;

		RenderText( ctx, m_largeFormat.Get(), title.str(), m_testTitleRect );

		PrintMetadata(ctx);
	}

#if 0
	std::wstringstream title;
	title << L"3.b Full-frame white for 30 minutes: ";
	if (0.0f != m_testTimeRemainingSec)
	{
		title << static_cast<unsigned int>(m_testTimeRemainingSec);
		title << L" seconds remaining";
		title << L"\nRenders HDR10 max (10,000 nits)";
		title << L"\nNits: ";
		title << nits;
		title << L"  HDR10: ";
		title << static_cast<unsigned int>(Apply2084(c*80.f / 10000.f) * 1023);
		title << L"\n" << m_hideTextString;
	}
	else
	{
		title << L" done.";
	}
	RenderText(ctx, m_largeFormat.Get(), title.str(), m_testTitleRect, true);
#endif
	m_newTestSelected = false;

}

void Game::GenerateTestPattern_LocalDimmingContrast(ID2D1DeviceContext2* ctx)				// v1.2.1
{
	ComPtr<ID2D1SolidColorBrush> barBrush;								// for white bars

	// Compute box intensity based on EDID
	float nits = m_outputDesc.MaxLuminance;
	float avg = nits * 0.20f;        // 20% screen area

	if (m_newTestSelected)
		SetMetadata(nits, avg, GAMUT_Native);

	auto rsc = m_testPatternResources[TestPattern::TenPercentPeak];
	std::wstringstream title;
	float dpi = m_deviceResources->GetDpi();
	auto logSize = m_deviceResources->GetLogicalSize();
	float fSizeW = (logSize.right - logSize.left) * 0.03f;				// bar is 3% screen width
	float fSizeH = (logSize.bottom - logSize.top) * 0.03f;				// bar is 3% screen height
	float2 center;
	center.x = (logSize.right - logSize.left) * 0.50f;
	center.y = (logSize.bottom - logSize.top) * 0.50f;
	float screenSize = sqrt((logSize.right - logSize.left) * (logSize.bottom - logSize.top));
	ComPtr<ID2D1SolidColorBrush> centerBrush;
	float c = nitstoCCCS(nits);
	DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF(c, c, c), &centerBrush));

	switch (m_LocalDimmingBars)
	{

	case 0:				// 1-D local dimming test image
	{
		// Draw the background noise effect
		if (!rsc.effectIsValid)
		{
			title << L"\nERROR: " << rsc.effectShaderFilename << L" is missing\n";
		}
		else
		{
			float APL = 5.0f;
			float limit = 10.0f;
			// ******* Arguments to this call are not type checked  *********
			// ******* DO NOT PASS A DOUBLE HERE (EVEN LITERAL!) *** ELSE FAIL ************* 
			rsc.d2dEffect->SetValueByName(L"APL", APL);		// ****** MUST BE FLOAT!
			rsc.d2dEffect->SetValueByName(L"Clamp", limit);									// nits
			rsc.d2dEffect->SetValueByName(L"iTime", m_totalTime);							// seconds

			ctx->DrawImage(rsc.d2dEffect.Get());
		}

		// lower left black box
		D2D1_RECT_F LL_Rect =
		{
			logSize.left,				// xmin
			logSize.top + center.y,		// ymin
			logSize.left + center.x,	// xmax
			logSize.bottom				// ymax
		};
		ctx->FillRectangle(&LL_Rect, m_blackBrush.Get());

		// upper right black box
		D2D1_RECT_F UR_Rect =
		{
			logSize.left + center.x,	// xmin
			logSize.top,				// ymin
			logSize.right,				// xmax
			logSize.top + center.y,		// ymax
		};
		ctx->FillRectangle(&UR_Rect, m_blackBrush.Get());

		// 8% screen area center box
		float patchPct = PATCHPCT;
		float size = screenSize * sqrtf(patchPct);  // dimensions for a square of this % screen area

		float2 jitter;
		float radius = JITTER_RADIUS * dpi / 96.0f;

		do {
			jitter.x = radius * (randf_s() * 2.f - 1.f);
			jitter.y = radius * (randf_s() * 2.f - 1.f);
		} while ((jitter.x * jitter.x + jitter.y * jitter.y) > radius);

		// Apply jitter
		float2 jcenter = center + jitter;

		D2D1_RECT_F centerRect =
		{
			jcenter.x - size * 0.50f,
			jcenter.y - size * 0.50f,
			jcenter.x + size * 0.50f,
			jcenter.y + size * 0.50f
		};
		ctx->FillRectangle(&centerRect, centerBrush.Get());
	}
	break;

	case 1:													// 2-D local dimming test image
	{
		// define the white bars
		c = nitstoCCCS(nits);
		DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF(c, c, c), &barBrush));

		D2D1_RECT_F leftRect =
		{
			logSize.left,				// xmin
			logSize.top,				// ymin
			logSize.left + fSizeW,		// xmax
			logSize.bottom,				// ymax
		};
		ctx->FillRectangle(&leftRect, barBrush.Get());

		D2D1_RECT_F rightRect =
		{
			logSize.right - fSizeW,		// xmin
			logSize.top,				// ymin
			logSize.right,				// xmax
			logSize.bottom				// ymax
		};
		ctx->FillRectangle(&rightRect, barBrush.Get());

		D2D1_RECT_F topRect =
		{
			logSize.left,				// xmin
			logSize.top,				// ymin
			logSize.right,				// xmax
			logSize.top + fSizeH,		// ymax
		};
		ctx->FillRectangle(&topRect, barBrush.Get());

		D2D1_RECT_F bottomRect =
		{
			logSize.left,				// xmin
			logSize.bottom - fSizeH,	// ymin
			logSize.right,				// xmax
			logSize.bottom				// ymax
		};
		ctx->FillRectangle(&bottomRect, barBrush.Get());

		float size = screenSize * sqrtf(0.08f);						// dimensions for a square of this % screen area
		D2D1_RECT_F BrightRect =
		{
			logSize.left + fSizeH,
			logSize.top + center.y - size * 0.50f,
			logSize.left + fSizeH  + size,
			logSize.top + center.y + size * 0.50f
		};
		ctx->FillRectangle(&BrightRect, barBrush.Get());
	}
	break;

	}

	// Everything below this point should be hidden for actual measurements.
	if (m_showExplanatoryText)
	{
		float size = screenSize * sqrtf(0.08f);						// dimensions for a square of this % screen area
		float fRad = 0.5f * m_snoodDiam / 25.4f * dpi * 1.2f;      // radius of dia 27mm -> inches -> dips
		D2D1_ELLIPSE ellipse;

		switch (m_LocalDimmingBars)
		{
		case 0:
			ellipse =
			{
				D2D1::Point2F(center.x, center.y),
				fRad, fRad
			};
			ctx->DrawEllipse(&ellipse, m_redBrush.Get(), 2.f );

			size = screenSize * sqrtf(0.05f);						// dimensions for a square of this % screen area

			ellipse =
			{
				D2D1::Point2F(logSize.right - size * 0.5f, logSize.top + size * 0.5f),
				fRad, fRad
			};
			ctx->DrawEllipse(&ellipse, m_redBrush.Get());

			ellipse =
			{
				D2D1::Point2F(logSize.left + size * 0.5f, logSize.bottom - size * 0.5f),
				fRad, fRad
			};
			ctx->DrawEllipse(&ellipse, m_redBrush.Get());
		break;

		case 1:
			ellipse =
			{
				D2D1::Point2F(logSize.left + fSizeH + size*0.5f, center.y),
				fRad, fRad
			};
			ctx->DrawEllipse(&ellipse, m_redBrush.Get(), 2.f );

			ellipse =
			{
				D2D1::Point2F(logSize.right - (center.y - fSizeH) - fSizeW, center.y),
				fRad, fRad
			};
			ctx->DrawEllipse(&ellipse, m_redBrush.Get());
		break;
		}

		title << fixed << setw(8) << setprecision(2);

		title << L"   Contrast test for Local Dimming: ";
		switch (m_LocalDimmingBars)
		{
		case 0: title << L" 1-D";
			break;
		case 1: title << L" 2-D";
			break;
		}
		title << L"\nNits: ";
		title << nits * BRIGHTNESS_SLIDER_FACTOR;
		title << L"  HDR10: ";
		title << setprecision(0);
		title << Apply2084(c * 80.f * BRIGHTNESS_SLIDER_FACTOR / 10000.f) * 1023.f;
		title << L"\nUp/Down arrows select 1D vs 2D dimming\n";
		title << m_hideTextString;

		// Shift title text to the right to avoid the corner.
		D2D1_RECT_F textRect = m_testTitleRect;
		RenderText(ctx, m_largeFormat.Get(), title.str(), textRect);

		PrintMetadata(ctx);
	}
	m_newTestSelected = false;

}
void Game::GenerateTestPattern_BlackLevelHDRvsSDR(ID2D1DeviceContext2* ctx)			   		// v1.2.2
{
	// Renders 10,000 nits. This is 125.0f CCCS or 1024 HDR10.
	ComPtr<ID2D1SolidColorBrush> leftBrush;

	// Compute box intensity based on EDID
	float nits = 200;
	float avg = nits * 0.5f;                // 50% screen area
	if (m_newTestSelected)
		SetMetadata( m_outputDesc.MaxLuminance, avg, GAMUT_Native);

	float c = nitstoCCCS(nits);
	DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF(c, c, c), &leftBrush));

	auto logSize = m_deviceResources->GetLogicalSize();
	D2D1_RECT_F leftRect =
	{
		logSize.left,
		logSize.top,
		logSize.right*0.5f,
		logSize.bottom
	};
	ctx->FillRectangle(&leftRect, leftBrush.Get());


	// Everything below this point should be hidden for actual measurements.
	if (m_showExplanatoryText)
	{
		float dpi = m_deviceResources->GetDpi();
		float fRad = 0.5f * m_snoodDiam / 25.4f * dpi * 1.2f;      // radius of dia 27mm -> inches -> dips

		float2 center = float2(logSize.right * 0.5f, logSize.bottom * 0.5f);

		D2D1_ELLIPSE ellipse =
		{
			D2D1::Point2F(logSize.right*0.25f, center.y),
			fRad, fRad
		};
		ctx->DrawEllipse(&ellipse, m_redBrush.Get(), 2.f);
		ellipse =
		{
			D2D1::Point2F(logSize.right * 0.75f, center.y),
			fRad, fRad
		};
		ctx->DrawEllipse(&ellipse, m_redBrush.Get());


		std::wstringstream title;
		title << fixed << setw(8) << setprecision(2);

		title << L"   Black Level in HDR vs SDR";
		title << L"\nNits: ";
		title << nits * BRIGHTNESS_SLIDER_FACTOR;
		title << L"  HDR10: ";
		title << setprecision(0);
		title << Apply2084(c * 80.f * BRIGHTNESS_SLIDER_FACTOR / 10000.f) * 1023.f;
		title << L"\n" << m_hideTextString;

		// Shift title text to the right to avoid the corner.
		D2D1_RECT_F textRect = m_testTitleRect;
		RenderText(ctx, m_largeFormat.Get(), title.str(), textRect);

		PrintMetadata(ctx);
	}
	m_newTestSelected = false;
}

void Game::GenerateTestPattern_BlackLevelCrush(ID2D1DeviceContext2* ctx)		       		// v1.2.3
{
	float nitsList[] = { 0.5f, 0.3f, 0.1f, 0.05f, 0.0f };
	float nits = nitsList[m_currentBlack];

	if (m_newTestSelected)
		SetMetadata(m_outputDesc.MaxLuminance, nits, GAMUT_Native);

	// Get fp16 color of this nit level
	float c = nitstoCCCS(nits) / BRIGHTNESS_SLIDER_FACTOR;

	// get a brush of that color
	ComPtr<ID2D1SolidColorBrush> darkBrush;
	DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF(c, c, c), &darkBrush));

	// get entire screen rect and fill it
	auto logSize = m_deviceResources->GetLogicalSize();
	ctx->FillRectangle(&logSize, darkBrush.Get());

	if (m_showExplanatoryText)
	{
		std::wstringstream title;
		title << L"   Black Level Crush Test: ";
		title << fixed << setw(8) << setprecision(2);
		title << L"\nNits: ";
		title << nits;
		title << L"  HDR10: ";
		title << setprecision(0);
		title << Apply2084(c * 80.f / 10000.f) * 1023.f;
		title << L"\n" << m_hideTextString;

		RenderText(ctx, m_largeFormat.Get(), title.str(), m_testTitleRect);

		PrintMetadata(ctx);
	}
	m_newTestSelected = false;
}

void Game::GenerateTestPattern_SubTitleFlicker(ID2D1DeviceContext2 * ctx)	        		// v1.2.4
{
	// Draw the center patch:
	float nits = 10.0f;		// per specification
	float c;
	c = nitstoCCCS(nits) / BRIGHTNESS_SLIDER_FACTOR;

	float avg = nits;
	if (m_newTestSelected)
		SetMetadata(m_outputDesc.MaxLuminance, avg, GAMUT_Native);

	// get bounds of entire window
	auto logSize = m_deviceResources->GetLogicalSize();
	std::wstringstream title;

	// Draw the background noise effect
	auto rsc = m_testPatternResources[TestPattern::TenPercentPeak];
	if (!rsc.effectIsValid)
	{
		title << L"\nERROR: " << rsc.effectShaderFilename << L" is missing\n";
	}
	else
	{
		float noiseAPL = 5.0f;
		float limit = 10.0f;
		// ******* Arguments to this call are not type checked  *********
		// ******* DO NOT PASS A DOUBLE HERE (EVEN LITERAL!) *** ELSE FAIL ************* 
		rsc.d2dEffect->SetValueByName(L"APL", noiseAPL );		// ****** MUST BE FLOAT!
		rsc.d2dEffect->SetValueByName(L"Clamp", limit );									// nits
		rsc.d2dEffect->SetValueByName(L"iTime", m_totalTime);								// seconds

		ctx->DrawImage(rsc.d2dEffect.Get());
	}

	// the center patch
	// same brightness as everything else
	ComPtr<ID2D1SolidColorBrush> centerBrush;
	DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF(c, c, c), &centerBrush));

	float size = sqrt((logSize.right - logSize.left) * (logSize.bottom - logSize.top));
	float patchPct = PATCHPCT;
	size = size * sqrtf(patchPct);  // dimensions for a square of this % screen area
	float2 center;
	center.x = (logSize.right - logSize.left) * 0.50f;
	center.y = (logSize.bottom - logSize.top) * 0.50f;

	float dpi = m_deviceResources->GetDpi();
	float2 jitter;
	float radius = JITTER_RADIUS * dpi / 96.0f;
	do {
		jitter.x = radius * (randf_s() * 2.f - 1.f);
		jitter.y = radius * (randf_s() * 2.f - 1.f);
	} while ((jitter.x * jitter.x + jitter.y * jitter.y) > radius);

	// Apply jitter
	center = center + jitter;

	D2D1_RECT_F centerRect =
	{
		center.x - size * 0.50f,
		center.y - size * 0.50f,
		center.x + size * 0.50f,
		center.y + size * 0.50f
	};
	ctx->FillRectangle(&centerRect, centerBrush.Get());

	// draw the Subtitle text
	if (m_subtitleVisible & 0x01)			// only switch visibility on last bit
	{
		// set the sub title color
		float textNits = 200.0f;			// bright white
		float cs = nitstoCCCS(textNits) / BRIGHTNESS_SLIDER_FACTOR;

		ComPtr<ID2D1SolidColorBrush> subtitleBrush;
		DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF(cs, cs, cs), &subtitleBrush));

		std::wstring subtitle = L"This is a subtitle text string for testing";
		float ctrx = (logSize.right + logSize.left) * 0.5f;
		D2D1_RECT_F subtitleRect =
		{
			ctrx - 500.0f, logSize.bottom - 150.f,
			ctrx + 600.0f, logSize.bottom
		};

		// create the text format
		Microsoft::WRL::ComPtr<IDWriteTextFormat> subtitleFormat;
		auto dwFactory = m_deviceResources->GetDWriteFactory();
		DX::ThrowIfFailed(dwFactory->CreateTextFormat(
			L"Segoe UI",
			nullptr,
			DWRITE_FONT_WEIGHT_NORMAL,
			DWRITE_FONT_STYLE_NORMAL,
			DWRITE_FONT_STRETCH_NORMAL,
			64.0f,
			L"en-US",
			&subtitleFormat));
		
		// define a text layout
		ComPtr<IDWriteTextLayout> layout;
		DX::ThrowIfFailed(dwFactory->CreateTextLayout(
			subtitle.c_str(),
			(unsigned int)subtitle.length(),
			subtitleFormat.Get(),
			subtitleRect.right,
			subtitleRect.bottom,
			&layout));

		// draw the subtitle with a black dropshadow underneath
		ctx->DrawTextLayout(D2D1::Point2F(subtitleRect.left + 1, subtitleRect.top + 1.f), layout.Get(), m_blackBrush.Get());
		ctx->DrawTextLayout(D2D1::Point2F(subtitleRect.left    , subtitleRect.top)      , layout.Get(), subtitleBrush.Get());
	}

	if (m_showExplanatoryText)
	{
		title << L"1.2.4 Subtitle Flicker Test: ";
		title << fixed << setw(8) << setprecision(2);
		title << L"\nNits: ";
		title << nits;
		title << L"  HDR10: ";
		title << setprecision(0);
		title << Apply2084(c * 80.f / 10000.f) * 1023.f;
		title << L"\n<Ctrl> key toggles subtitles";
		title << L"\n" << m_hideTextString;

		RenderText(ctx, m_largeFormat.Get(), title.str(), m_testTitleRect);

		PrintMetadata(ctx);
	}

	m_newTestSelected = false;
}

void Game::GenerateTestPattern_XRiteColors(ID2D1DeviceContext2* ctx)						// v1.2.5
{
	if (m_newTestSelected)
	{
		SetMetadata(m_outputDesc.MaxLuminance, m_outputDesc.MaxLuminance * 0.10f, GAMUT_Native);
		m_testTimeRemainingSec = 1.0f;			// 1 seconds  be sure we are set up for animation
	}

	float3 colorXYZ;
	colorXYZ.x = (float)XRite[m_currentXRiteIndex].X/100.f;
	colorXYZ.y = (float)XRite[m_currentXRiteIndex].Y/100.f;
	colorXYZ.z = (float)XRite[m_currentXRiteIndex].Z/100.f;

	// convert to 709 gamut space	
	float3 color709 = XYZ_to_709RGB * colorXYZ;

#if 0
	if (CheckHDR_On())
		ColorCCCS = Apply2084(Color709);
	else
		ColorCCCS = ApplySRGBCurve(Color709);
#endif

	float3 colorHDR10;
	colorHDR10.r = XRite[m_currentXRiteIndex].R;
	colorHDR10.g = XRite[m_currentXRiteIndex].G;
	colorHDR10.b = XRite[m_currentXRiteIndex].B;
	
	// scale up for white level selected
//	colorHDR10 = colorHDR10 * (float)(0.01f*WhiteLevelBrackets[m_whiteLevelBracket]);

	// convert to Canonical Composition Color Space for desktop compositor
	float3 colorCCCS = HDR10ToLinear709( colorHDR10 );

	// scale up for white level selected now we are linear
	colorCCCS = colorCCCS * (float)(0.01f * WhiteLevelBrackets[m_whiteLevelBracket]);


	float nits = Remove2084( 1023.f / 1023.0f) * 10000.0f;		// go to linear space
//	float c = nitstoCCCS(nits / BRIGHTNESS_SLIDER_FACTOR);		// scale by 80 and slider

	std::wstringstream title;
	auto logSize = m_deviceResources->GetLogicalSize();

// Draw the background noise effect
	auto rsc = m_testPatternResources[TestPattern::TenPercentPeak];
	if (!rsc.effectIsValid)
	{
		title << L"\nERROR: " << rsc.effectShaderFilename << L" is missing\n";
	}
	else
	{
		float TierMaxLum = GetTierLuminance(m_testingTier);
		float noiseAPL = TierMaxLum * 0.02174f;
		float limit = 100.0f;
		// ******* Arguments to these calls are not type checked  *********
		// ******* DO NOT PASS A DOUBLE HERE (EVEN LITERAL!) *** ELSE FAIL ************* 
		rsc.d2dEffect->SetValueByName(L"APL", noiseAPL);	// ****** MUST BE FLOAT!		// fraction of screen
		rsc.d2dEffect->SetValueByName(L"Clamp", limit );									// nits
		rsc.d2dEffect->SetValueByName(L"iTime", m_totalTime);								// seconds

		ctx->DrawImage(rsc.d2dEffect.Get());
	}


	// create D2D brush of this color
	ComPtr<ID2D1SolidColorBrush> centerBrush;
	DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF(colorCCCS.r, colorCCCS.g, colorCCCS.b), &centerBrush));

	float size = sqrt((logSize.right - logSize.left) * (logSize.bottom - logSize.top));
	float patchPct = PATCHPCT;
	size = size * sqrtf(patchPct);  // dimensions for a square of this % screen area
	float2 center;
	center.x = (logSize.right - logSize.left) * 0.50f;
	center.y = (logSize.bottom - logSize.top) * 0.50f;

	float dpi = m_deviceResources->GetDpi();
	float2 jitter;
	float radius = JITTER_RADIUS * dpi / 96.0f;
	do {
		jitter.x = radius * (randf_s() * 2.f - 1.f);
		jitter.y = radius * (randf_s() * 2.f - 1.f);
	} while ((jitter.x * jitter.x + jitter.y * jitter.y) > radius);

	// Apply jitter
	center = center + jitter;

	D2D1_RECT_F centerRect =
	{
		center.x - size * 0.50f,
		center.y - size * 0.50f,
		center.x + size * 0.50f,
		center.y + size * 0.50f
	};
	ctx->FillRectangle(&centerRect, centerBrush.Get());

	if (m_showExplanatoryText)
	{
		float fRad = 0.5f * m_snoodDiam / 25.4f * dpi * 1.2f;      // radius of dia 27mm -> inches -> dips
		float2 center = float2(logSize.right * 0.5f, logSize.bottom * 0.5f);

		D2D1_ELLIPSE ellipse =
		{
			D2D1::Point2F(center.x, center.y),
			fRad, fRad						// TODO: make target diameter 27mm using DPI data
		};

		ctx->DrawEllipse(&ellipse, m_redBrush.Get(), 2 );

		title << fixed << setprecision(0);

		title << L"   X-Rite™ Colors\n";
		title << L"Color#: ";
		title << setw(3) << XRite[m_currentXRiteIndex].num;
		title << L"   RxC: ";
		title << setw(2) << XRite[m_currentXRiteIndex].row;
		title << L"|";
		title << XRite[m_currentXRiteIndex].col;
		title << setbase(16) << L"   RGB  ";
		title << setw(6) << XRite[m_currentXRiteIndex].RGB;
		title << setbase(10) << L"\n White Level: ";
		title << WhiteLevelBrackets[m_whiteLevelBracket];
		title << L" - use [ ] to select bracket";
		title << L"\nSelect color via Up/Down arrow keys";
		title << setprecision(1) << L"\nAuto advance using 'A' key. Advances every " << m_XRitePatchDisplayTime << "sec";
		title << "\nChange timing using +/- keys";
		if (m_XRitePatchAutoMode)
			title << L"\nAuto Advance";
		title << L"\n";
		title << m_hideTextString;

		RenderText(ctx, m_largeFormat.Get(), title.str(), m_testTitleRect);

		PrintMetadata(ctx);
	}

#if 0
	std::wstringstream title;
	title << L"3.b Full-frame white for 30 minutes: ";
	if (0.0f != m_testTimeRemainingSec)
	{
		title << static_cast<unsigned int>(m_testTimeRemainingSec);
		title << L" seconds remaining";
		title << L"\nRenders HDR10 max (10,000 nits)";
		title << L"\nNits: ";
		title << nits;
		title << L"  HDR10: ";
		title << static_cast<unsigned int>(Apply2084(c * 80.f / 10000.f) * 1023);
		title << L"\n" << m_hideTextString;
	}
	else
	{
		title << L" done.";
	}
	RenderText(ctx, m_largeFormat.Get(), title.str(), m_testTitleRect, true);
#endif
	m_newTestSelected = false;

}


void Game::GenerateTestPattern_EndOfMandatoryTests(ID2D1DeviceContext2 * ctx)
{
    if (m_newTestSelected) SetMetadataNeutral();

    std::wstringstream text;
    text << L"This is the end of mandatory test content.\n";
    text << L"Optional/informative tests follow.";

    RenderText(ctx, m_largeFormat.Get(), text.str(), m_largeTextRect);
    m_newTestSelected = false;
}

void Game::GenerateTestPattern_SharpeningFilter(ID2D1DeviceContext2* ctx)
{
	auto rsc = m_testPatternResources[TestPattern::SharpeningFilter];

	// Overload the color selector to allow selecting the desired SDR white level.
	float nits = 80.0f;
	switch (m_currentColor)
	{
	case 0:
		nits = 80.0f;
		break;
	case 1:
		nits = 160.0f;
		break;
	case 2:
		nits = 240.0f;
		break;
	case 3:
		nits = 320.0f;
		break;
	default:
		DX::ThrowIfFailed(E_INVALIDARG);
		break;
	}
	if (m_newTestSelected)
		SetMetadata(m_outputDesc.MaxLuminance, nits * 0.50f, GAMUT_Native); SetMetadataNeutral();

	std::wstringstream title;

	// TODO: Convert to a generalized custom effect pipeline.
	if (!rsc.effectIsValid)
	{
		title << L"\nERROR: " << rsc.effectShaderFilename << L" is missing\n";
	}
	else
	{
		auto out = m_deviceResources->GetOutputSize();
		D2D1_POINT_2F center = { (out.right - out.left) / 2.0f, (out.bottom - out.top) / 2.0f };

		// Initial wavelength of sine wave at the centerpoint.
		float init = 30.0f;

		// How many pixels before the wavelength halves.
		float dist = (out.right - out.left) / 4.0f;

		rsc.d2dEffect->SetValueByName(L"Center", center);
		rsc.d2dEffect->SetValueByName(L"InitialWavelength", init);
		rsc.d2dEffect->SetValueByName(L"WavelengthHalvingDistance", dist);
		rsc.d2dEffect->SetValueByName(L"WhiteLevelMultiplier", nitstoCCCS(nits));

		ctx->DrawImage(rsc.d2dEffect.Get());
	}

	if (m_showExplanatoryText)
	{
		ComPtr<ID2D1SolidColorBrush> blackBrush;
		DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.0f, 0.0f), &blackBrush));
		ctx->FillRectangle(m_testTitleRect, blackBrush.Get());
		title << fixed << setw(8) << setprecision(2);
		title << rsc.testTitle << "\nUsing reference level of : " << nits;
		title << " nits.\nUp & Down arrow keys select white level.\n";
		RenderText(ctx, m_largeFormat.Get(), title.str(), m_testTitleRect);

		PrintMetadata(ctx);
	}

	m_newTestSelected = false;
}

void Game::GenerateTestPattern_ToneMapSpike(ID2D1DeviceContext2 * ctx)
{
    auto rsc = m_testPatternResources[TestPattern::ToneMapSpike];

    // Overload the color selector to set the level we tone map the content to
    float nits = 80.0f;
    switch (m_currentColor)
    {
    case 0:
//		nits = m_outputDesc.MaxLuminance;
		nits = 350.0f;
		SetMetadata(nits, nits*0.1f, GAMUT_Native);
        break;
    case 1:
        nits = 700.0f;
		SetMetadata(nits, nits * 0.1f, GAMUT_Native);
		break;
    case 2:
        nits = 1015.0f;
		SetMetadata(nits, nits * 0.1f, GAMUT_Native);
		break;
    case 3:
        nits = 10000.0f;
		SetMetadata(nits, nits * 0.1f, GAMUT_Native);
        break;
    default:
        DX::ThrowIfFailed(E_INVALIDARG);
        break;
    }
//	if (m_newTestSelected)
//		SetMetadata(m_outputDesc.MaxLuminance, nits*0.50f, GAMUT_Native); SetMetadataNeutral();

    std::wstringstream title;

    // TODO: Convert to a generalized custom effect pipeline.
	D2D1_POINT_2F center;
	center.x = 640.f;
	center.y = 360.f;

	if (!rsc.effectIsValid)
    {
        title << L"\nERROR: " << rsc.effectShaderFilename << L" is missing\n";
    }
    else
    {
        auto out = m_deviceResources->GetOutputSize();
        center = { (out.right - out.left) / 2.0f, (out.bottom - out.top) / 2.0f };

        // Initial wavelength of sine wave at the centerpoint.
        float init = 30.0f;

        // How many pixels before the wavelength halves.
        float dist = (out.right - out.left) / 4.0f;

        rsc.d2dEffect->SetValueByName(L"Center", center);
        rsc.d2dEffect->SetValueByName(L"InitialWavelength", init);
        rsc.d2dEffect->SetValueByName(L"WavelengthHalvingDistance", dist);
        rsc.d2dEffect->SetValueByName(L"WhiteLevelMultiplier", nitstoCCCS(nits));

        ctx->DrawImage(rsc.d2dEffect.Get());
    }

    if (m_showExplanatoryText)
    {
		auto logSize = m_deviceResources->GetLogicalSize();

		float fRad = sqrtf(center.x * center.y *4.0f / M_PI_F);	// 100% screen area circle
		D2D1_ELLIPSE ellipse =
		{
			center, fRad, fRad
		};
		ctx->DrawEllipse(&ellipse, m_redBrush.Get(), 1);

		float OPR = 0.10f;									// 10% screen area for maxLuminance
		float fR = fRad * sqrt(OPR);
		D2D1_ELLIPSE ellipse10 =
		{
			center, fR, fR
		};
		ctx->DrawEllipse(&ellipse10, m_redBrush.Get(), 2);

		OPR = 0.0001f;									// 0.01% screen area for sun (noise rejection  limit)
		fR = fRad * sqrt(OPR);
		D2D1_ELLIPSE ellipse0001 =
		{
			center, fR, fR
		};
		ctx->DrawEllipse(&ellipse0001, m_redBrush.Get(), 3);
		
		ComPtr<ID2D1SolidColorBrush> blackBrush;
        DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.0f, 0.0f), &blackBrush));
        ctx->FillRectangle(m_testTitleRect, blackBrush.Get());
		title << fixed << setw(8) << setprecision(0);
        title << rsc.testTitle << "\nTone Mapping to : "  << nits;
        title << " nits.\nUp & Down arrow keys select level.\n";
        RenderText(ctx, m_largeFormat.Get(), title.str(), m_testTitleRect);

		PrintMetadata(ctx);
	}

    m_newTestSelected = false;
}

void Game::GenerateTestPattern_StaticGradient(ID2D1DeviceContext2* ctx)
{
    if (m_newTestSelected) SetMetadataNeutral();

    // TODO: All of the gradients share the below 2 lines.
    auto rect = m_deviceResources->GetLogicalSize();
    ctx->FillRectangle(&rect, m_gradientBrush.Get());

    // Everything below this point should be hidden for actual measurements.
    if (m_showExplanatoryText)
    {

        std::wstringstream text;
        text << L"Static Gradient: R(" << DX::to_string_with_precision(m_gradientColor.r)
            << L") G(" << DX::to_string_with_precision(m_gradientColor.g)
            << L") B(" << DX::to_string_with_precision(m_gradientColor.b)
            << L")\n";
        text << L"A/S: increment gradient color\n" << m_hideTextString;

        RenderText(ctx, m_largeFormat.Get(), text.str(), m_testTitleRect);

		PrintMetadata(ctx);
	}
    m_newTestSelected = false;

}

void Game::GenerateTestPattern_AnimatedGrayGradient(ID2D1DeviceContext2* ctx)
{
    if (m_newTestSelected) SetMetadataNeutral();

    // TODO: All of the gradients share the below 2 lines.
    auto rect = m_deviceResources->GetLogicalSize();
    ctx->FillRectangle(&rect, m_gradientBrush.Get());

    // Everything below this point should be hidden for actual measurements.
    if (m_showExplanatoryText)
    {

        std::wstring text = L"Animated Gray Gradient\n" + m_hideTextString;
        RenderText(ctx, m_largeFormat.Get(), text, m_testTitleRect);

		PrintMetadata(ctx);
	}
    m_newTestSelected = false;

}

void Game::GenerateTestPattern_AnimatedColorGradient(ID2D1DeviceContext2* ctx)
{
    if (m_newTestSelected) SetMetadataNeutral();

    // TODO: All of the gradients share the below 2 lines.
    auto rect = m_deviceResources->GetLogicalSize();
    ctx->FillRectangle(&rect, m_gradientBrush.Get());

    // Everything below this point should be hidden for actual measurements.
    if (m_showExplanatoryText)
    {

        std::wstring text = L"Animated Color Gradient\n" + m_hideTextString;
        RenderText(ctx, m_largeFormat.Get(), text, m_testTitleRect);

		PrintMetadata(ctx);
	}
    m_newTestSelected = false;

}

void Game::GenerateTestPattern_FullFrameSDRWhite(ID2D1DeviceContext2 * ctx)
{
    // Default metadata is used because this simulates a desktop environment.
    if (m_newTestSelected) SetMetadataNeutral();

    // Overload the color selector to allow selecting the desired SDR white level.
    float nits = 80.0f;
    switch (m_currentColor)
    {
    case 0:
        nits = 80.0f;
        break;
    case 1:
        nits = 160.0f;
        break;
    case 2:
        nits = 240.0f;
        break;
    case 3:
        nits = 320.0f;
        break;
    default:
        DX::ThrowIfFailed(E_INVALIDARG);
        break;
    }

    ComPtr<ID2D1SolidColorBrush> whiteBrush;
    DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF(nitstoCCCS(nits), nitstoCCCS(nits), nitstoCCCS(nits)), &whiteBrush));
    ctx->FillRectangle(m_deviceResources->GetLogicalSize(), whiteBrush.Get());

    if (m_showExplanatoryText)
    {
        std::wstringstream title;
		title << fixed << setw(8) << setprecision(2);
        title << L"Full frame SDR white\nUsing reference level of: " << nits;
        title << " nits.\nUp & Down arrow keys select white level.\n";

        RenderText(ctx, m_largeFormat.Get(), title.str(), m_testTitleRect, true);

		PrintMetadata(ctx, true);
	}

    m_newTestSelected = false;
}

void Game::GenerateTestPattern_FullFrameSDRWhiteWithHDR(ID2D1DeviceContext2 * ctx)
{
    // Default metadata is used because this simulates a desktop environment.
    if (m_newTestSelected) SetMetadataNeutral();

    // Overload the color selector to allow selecting the desired HDR highlight level.
    float nits = 80.0f;
    switch (m_currentColor)
    {
    case 0:
        // Default case is to use the display's reported max nits.
        nits = m_outputDesc.MaxLuminance;
        break;
    case 1:
        nits = 600.0f;
        break;
    case 2:
        nits = 1000.0f;
        break;
    case 3:
        nits = 1400.0f;
        break;
    default:
        DX::ThrowIfFailed(E_INVALIDARG);
        break;
    }

    // SDR full frame background is locked at 240 nits.
    float sdrNits = 240.0f;

    ComPtr<ID2D1SolidColorBrush> sdrBrush;
    DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF(nitstoCCCS(sdrNits), nitstoCCCS(sdrNits), nitstoCCCS(sdrNits)), &sdrBrush));

    ComPtr<ID2D1SolidColorBrush> hdrBrush;
    DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF(nitstoCCCS(nits), nitstoCCCS(nits), nitstoCCCS(nits)), &hdrBrush));

    auto logSize = m_deviceResources->GetLogicalSize();
    D2D1_RECT_F tenPercentRect =
    {
        (logSize.right - logSize.left) * (0.5f - sqrtf(0.1) / 2.0f),
        (logSize.bottom - logSize.top) * (0.5f - sqrtf(0.1) / 2.0f),
        (logSize.right - logSize.left) * (0.5f + sqrtf(0.1) / 2.0f),
        (logSize.bottom - logSize.top) * (0.5f + sqrtf(0.1) / 2.0f)
    };

    ctx->FillRectangle(&logSize, sdrBrush.Get());
    ctx->FillRectangle(&tenPercentRect, hdrBrush.Get());

    if (m_showExplanatoryText)
    {
        std::wstringstream title;
		title << fixed << setw(8) << setprecision(2);
        title << L"Full frame SDR white with 10% HDR window (" << nits;
        title << " nits).\nUp & Down arrow keys select HDR nits.\n";

        RenderText(ctx, m_largeFormat.Get(), title.str(), m_testTitleRect, true);

		PrintMetadata(ctx, true );
	}

    m_newTestSelected = false;
}


// Common method to render an image test pattern to the screen.
void Game::GenerateTestPattern_ImageCommon(ID2D1DeviceContext2* ctx, TestPatternResources resources)
{
    // SetMetadata depending on if we tone mapped the image or not
    if (m_newTestSelected) SetMetadataNeutral();

    if (resources.imageIsValid == true)
    {
        // Center the image, draw at 1.0x (pixel) zoom.
        // TODO: Currently we force all D2D rendering to 96 DPI regardless of display DPI.
        // Therefore DIPs = pixels.
        auto targetSize = m_deviceResources->GetOutputSize();
        unsigned int width, height;
        DX::ThrowIfFailed(resources.wicSource->GetSize(&width, &height));

        float dX = (targetSize.right - targetSize.left - static_cast<float>(width)) / 2.0f;
        float dY = (targetSize.bottom - targetSize.top - static_cast<float>(height)) / 2.0f;

        ctx->DrawImage(resources.d2dSource.Get(), D2D1::Point2F(dX, dY));
    }

    // Everything below this point should be hidden for actual measurements.
    if (m_showExplanatoryText)
    {
        std::wstringstream text;
        text << resources.testTitle << L"\n" << m_hideTextString;

        if (resources.imageIsValid == false)
        {
            text << L"\nERROR: " << resources.imageFilename << " is missing.";
        }

        RenderText(ctx, m_largeFormat.Get(), text.str(), m_testTitleRect);
    }

    m_newTestSelected = false;
}

void Game::GenerateTestPattern_EndOfTest(ID2D1DeviceContext2* ctx)
{
    std::wstringstream text;
    text << L"This is the end of the test content.\n";
    text << L"Press ALT-F4 to quit.";

    RenderText(ctx, m_largeFormat.Get(), text.str(), m_largeTextRect);
    m_newTestSelected = false;

}
#pragma endregion

#pragma region Frame Render
// Draws the scene.
void Game::Render()
{
    // Don't try to render anything before the first Update.
    if (m_timer.GetFrameCount() == 0)
    {
        return;
    }

    m_deviceResources->PIXBeginEvent(L"Render");

    Clear();

    auto ctx = m_deviceResources->GetD2DDeviceContext();

    ctx->BeginDraw();

    // Do test pattern-specific rendering here.
    // RenderD2D() handles all operations that are common to all test patterns:
    // 1. BeginDraw/EndDraw
    switch (m_currentTest)
    {
    case TestPattern::StartOfTest:
        GenerateTestPattern_StartOfTest(ctx);
        break;
    case TestPattern::ConnectionProperties:
        GenerateTestPattern_ConnectionProperties(ctx);
        break;
    case TestPattern::PanelCharacteristics:
        GenerateTestPattern_PanelCharacteristics(ctx);
        break;
    case TestPattern::ResetInstructions:
        GenerateTestPattern_ResetInstructions(ctx);
        break;
	case TestPattern::PQLevelsInNits:
        GenerateTestPattern_PQLevelsInNits(ctx);
        break;
    case TestPattern::WarmUp:
        GenerateTestPattern_WarmUp(ctx);
        break;
    case TestPattern::TenPercentPeak:									// 1.
        GenerateTestPattern_TenPercentPeak(ctx);
        break;
    case TestPattern::TenPercentPeakMAX:								// 1.MAX
        GenerateTestPattern_TenPercentPeakMAX(ctx);
        break;
    case TestPattern::FlashTest:										// 2.
        GenerateTestPattern_FlashTest(ctx);
        break;
    case TestPattern::FlashTestMAX:										// 2.MAX
        GenerateTestPattern_FlashTestMAX(ctx);
        break;
	case TestPattern::LongDurationWhite:								// 3.
		GenerateTestPattern_LongDurationWhite(ctx);
		break;
	case TestPattern::FullFramePeak:									// 3.MAX
        GenerateTestPattern_FullFramePeak(ctx);
        break;
    case TestPattern::BlackLevelHdrCorners:								// legacy
        GenerateTestPattern_BlackLevelHdrCorners(ctx);
        break;
	case TestPattern::DualCornerBox:									// 4.
		GenerateTestPattern_DualCornerBox(ctx);
		break;
	case TestPattern::StaticContrastRatio:								// 5.
        GenerateTestPattern_StaticContrastRatio(ctx);
        break;
	case TestPattern::ActiveDimming:									// 5.1
		GenerateTestPattern_ActiveDimming(ctx);
		break;
	case TestPattern::ActiveDimmingDark:								// 5.2
		GenerateTestPattern_ActiveDimmingDark(ctx);
		break;
	case TestPattern::ActiveDimmingSplit:								// 5.3
		GenerateTestPattern_ActiveDimmingSplit(ctx);
		break;
	case TestPattern::ColorPatches:										// 6. with 10% screen coverage
		GenerateTestPattern_ColorPatches(ctx, false);
		break;
	case TestPattern::ColorPatchesFull:									// 6. with 100% screen coverage
		GenerateTestPattern_ColorPatches(ctx, true );
		break;
    case TestPattern::BitDepthPrecision:								// 7.
        GenerateTestPattern_BitDepthPrecision(ctx);
        break;
    case TestPattern::RiseFallTime:										// 8.
        GenerateTestPattern_RiseFallTime(ctx);
        break;
	case TestPattern::ProfileCurve:										// 9.
		GenerateTestPattern_ProfileCurve(ctx);
		break;
	case TestPattern::LocalDimmingContrast:								// v1.2   Four new tests for v1.2
		GenerateTestPattern_LocalDimmingContrast(ctx);
		break;
	case TestPattern::BlackLevelHDRvsSDR:								// B
		GenerateTestPattern_BlackLevelHDRvsSDR(ctx);
		break;
	case TestPattern::BlackLevelCrush:									// C
		GenerateTestPattern_BlackLevelCrush(ctx);
		break;
	case TestPattern::SubTitleFlicker:									// D
		GenerateTestPattern_SubTitleFlicker(ctx);
		break;
	case TestPattern::XRiteColors:										// v1.2 
		GenerateTestPattern_XRiteColors(ctx);
		break;
	case TestPattern::EndOfMandatoryTests:								// Marker for end of mandatory tests
        GenerateTestPattern_EndOfMandatoryTests(ctx);
        break;
    case TestPattern::SharpeningFilter:									// Sine Sweep Effect
        GenerateTestPattern_SharpeningFilter(ctx);
        break;
	case TestPattern::ToneMapSpike:										// test tone mapping
		GenerateTestPattern_ToneMapSpike(ctx);
		break;
	case TestPattern::TextQuality:
        GenerateTestPattern_ImageCommon(ctx, m_testPatternResources[TestPattern::TextQuality]);
        break;
    case TestPattern::OnePixelLinesBW:
        GenerateTestPattern_ImageCommon(ctx, m_testPatternResources[TestPattern::OnePixelLinesBW]);
        break;
    case TestPattern::OnePixelLinesRG:
        GenerateTestPattern_ImageCommon(ctx, m_testPatternResources[TestPattern::OnePixelLinesRG]);
        break;
    case TestPattern::ColorPatches709:
        GenerateTestPattern_ColorPatches709(ctx);
        break;
    case TestPattern::FullFrameSDRWhite:
        GenerateTestPattern_FullFrameSDRWhite(ctx);
        break;
    case TestPattern::FullFrameSDRWhiteWithHDR:
        GenerateTestPattern_FullFrameSDRWhiteWithHDR(ctx);
        break;
	case TestPattern::CalibrateMaxEffectiveValue:						// detect white crush
		GenerateTestPattern_CalibrateMaxEffectiveValue(ctx);
		break;
	case TestPattern::CalibrateMaxEffectiveFullFrameValue:				// check profile at FALL
		GenerateTestPattern_CalibrateMaxFullFrameValue(ctx);
		break;
	case TestPattern::CalibrateMinEffectiveValue:						// detect black crush
		GenerateTestPattern_CalibrateMinEffectiveValue(ctx);
		break;
    case TestPattern::StaticGradient:
        GenerateTestPattern_StaticGradient(ctx);
        break;
    case TestPattern::AnimatedGrayGradient:
        GenerateTestPattern_AnimatedGrayGradient(ctx);
        break;
    case TestPattern::AnimatedColorGradient:
        GenerateTestPattern_AnimatedColorGradient(ctx);
        break;
	case TestPattern::BlackLevelSdrTunnel:
		GenerateTestPattern_BlackLevelSdrTunnel(ctx);
		break;
	case TestPattern::ColorPatchesMAX:									// 6.MAX	legacy
		GenerateTestPattern_ColorPatchesMAX(ctx, 1.00);
		break;
	case TestPattern::EndOfTest:
        GenerateTestPattern_EndOfTest(ctx);
        break;
    case TestPattern::Cooldown:
        GenerateTestPattern_Cooldown(ctx);
        break;
    default:
        DX::ThrowIfFailed(E_NOTIMPL);
        break;
    }

    // Ignore D2DERR_RECREATE_TARGET here. This error indicates that the device
    // is lost. It will be handled during the next call to Present.
    HRESULT hr = ctx->EndDraw();
    if (hr != D2DERR_RECREATE_TARGET)
    {
        DX::ThrowIfFailed(hr);
    }

    m_deviceResources->PIXEndEvent();

    // Show the new frame.
    m_deviceResources->Present();
}

// Helper method to clear the back buffers.
void Game::Clear()
{
    m_deviceResources->PIXBeginEvent(L"Clear");

    // Clear the views.
    auto context = m_deviceResources->GetD3DDeviceContext();
    auto renderTarget = m_deviceResources->GetRenderTargetView();
    auto depthStencil = m_deviceResources->GetDepthStencilView();

    context->ClearRenderTargetView(renderTarget, Colors::Black);
    context->ClearDepthStencilView(depthStencil, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    context->OMSetRenderTargets(1, &renderTarget, depthStencil);

    // Set the viewport.
    auto viewport = m_deviceResources->GetScreenViewport();
    context->RSSetViewports(1, &viewport);

    m_deviceResources->PIXEndEvent();
}

// Note: textPos is interpreted as { Left, Top, ***Width***, ***Height*** } which is different from the
// definition of D2D1_RECT_F
void Game::RenderText(ID2D1DeviceContext2* ctx, IDWriteTextFormat* fmt, std::wstring text, D2D1_RECT_F textPos, bool useBlackText /* = false */)
{
    auto fact = m_deviceResources->GetDWriteFactory();
    ComPtr<IDWriteTextLayout> layout;
    DX::ThrowIfFailed(fact->CreateTextLayout(
        text.c_str(),
        (unsigned int)text.length(),
        fmt,
        textPos.right,
        textPos.bottom,
        &layout));

	if (useBlackText)
    {
		// add highlight
//		ctx->DrawTextLayout(D2D1::Point2F(textPos.left, textPos.top), layout.Get(), m_whiteBrush.Get());
		ctx->DrawTextLayout(D2D1::Point2F(textPos.left+1.f, textPos.top+1.f), layout.Get(), m_blackBrush.Get());
    }
    else
    {
		// add dropshadow
		ctx->DrawTextLayout(D2D1::Point2F(textPos.left + 1.f, textPos.top + 1.f), layout.Get(), m_blackBrush.Get());
		ctx->DrawTextLayout(D2D1::Point2F(textPos.left,       textPos.top),       layout.Get(), m_whiteBrush.Get());
    }



}
#pragma endregion

#pragma region Message Handlers
// Message handlers
void Game::OnActivated()
{
    // TODO: Game is becoming active window.
}

void Game::OnDeactivated()
{
    // TODO: Game is becoming background window.
}

void Game::OnSuspending()
{
    // TODO: Game is being power-suspended (or minimized).
}

void Game::OnResuming()
{
    m_timer.ResetElapsedTime();

    // TODO: Game is being power-resumed (or returning from minimize).
}

void Game::OnWindowSizeChanged(int width, int height)
{
    // Window size changed also corresponds to switching monitors.
    m_dxgiColorInfoStale = true;

    if (!m_deviceResources->WindowSizeChanged(width, height))
        return;

    CreateWindowSizeDependentResources();
}

// Currently only updates DXGI_OUTPUT_DESC1 state.
void Game::OnDisplayChange()
{
    // Wait until the next Update call to refresh state.
    m_dxgiColorInfoStale = true;
}

// Properties
void Game::GetDefaultSize(int& width, int& height) const
{
    width = 1280;
    height = 720;
}
#pragma endregion

#pragma region Direct3D Resources
void Game::CreateDeviceIndependentResources()
{
    auto dwFactory = m_deviceResources->GetDWriteFactory();

    DX::ThrowIfFailed(dwFactory->CreateTextFormat(
        L"Segoe UI",
        nullptr,
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        14.0f,
        L"en-US",
        &m_smallFormat));

    DX::ThrowIfFailed(dwFactory->CreateTextFormat(
        L"Consolas",
        nullptr,
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        18.0f,
        L"en-US",
        &m_monospaceFormat));

    DX::ThrowIfFailed(dwFactory->CreateTextFormat(
        L"Segoe UI",
        nullptr,
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        24.0f,
        L"en-US",
        &m_largeFormat));

	DX::ThrowIfFailed(BackgroundNoiseEffect::Register(m_deviceResources->GetD2DFactory()));
    DX::ThrowIfFailed(BandedGradientEffect::Register(m_deviceResources->GetD2DFactory()));
	DX::ThrowIfFailed(SineSweepEffect::Register(m_deviceResources->GetD2DFactory()));
	DX::ThrowIfFailed(ToneSpikeEffect::Register(m_deviceResources->GetD2DFactory()));
}

// These are the resources that depend on the device.
void Game::CreateDeviceDependentResources()
{
    auto ctx = m_deviceResources->GetD2DDeviceContext();

    DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White, 1), &m_whiteBrush));
	DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black, 1), &m_blackBrush));
	DX::ThrowIfFailed(ctx->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Red, 1), &m_redBrush));

    for (auto it = m_testPatternResources.begin(); it != m_testPatternResources.end(); it++)
    {
        LoadTestPatternResources(&it->second);
    }

    UpdateDxgiColorimetryInfo();

	// Try to guess the testing tier that we are trying against
	m_testingTier = GetTestingTier();

	// get reasonable starting points for calibration
	InitEffectiveValues();

	m_activeDimming50PQValue = 113 * 4;		// 50.825 nits in nearest 8-bit code value
	m_activeDimming05PQValue = 64 * 4;		//  5.172 nits in nearest 8-bit code value

}

// Allocate all memory resources that change on a window SizeChanged event.
void Game::CreateWindowSizeDependentResources()
{
    // Images are not scaled for window size - they are preserved at 1:1 pixel size.

    // D2D1_RECT_F is defined as: Left, Left, Right, Bottom
    // But we will interpret the struct members as: Left, Left, Width, Height
    // This lets us pack the size (for IDWriteTextLayout) and offset (for DrawText)
    // into a single struct.
    m_testTitleRect = { 0.0f, 0.0f, 800.0f, 100.0f };

	auto logicalSize = m_deviceResources->GetLogicalSize();

    m_largeTextRect =
    {
        (logicalSize.right - logicalSize.left) * 0.15f,
        (logicalSize.bottom - logicalSize.top) * 0.15f,
        (logicalSize.right - logicalSize.left) * 0.75f,
        (logicalSize.bottom - logicalSize.top) * 0.75f
    };

	m_MetadataTextRect =
	{
		logicalSize.right - 700.0f,
		logicalSize.bottom - 35.0f,
		logicalSize.right,
		logicalSize.bottom
	};

	m_TestingTierTextRect =
	{
		logicalSize.right - 200.0f,
		logicalSize.top,
		logicalSize.right,
		logicalSize.top + 35.f
	};
}

// This loads both device independent and dependent resources for the test pattern.
// Relies on wicSource and d2dSource being nullptr to (re)load resources.
void Game::LoadTestPatternResources(TestPatternResources* resources)
{
    LoadImageResources(resources);
    LoadEffectResources(resources);
}

void Game::LoadImageResources(TestPatternResources* resources)
{
    auto wicFactory = m_deviceResources->GetWicImagingFactory();
    auto ctx = m_deviceResources->GetD2DDeviceContext();

    // This test involves an image file.
    if (resources->imageFilename.compare(L"") != 0)
    {
        // First, ensure that there is a WIC source (device independent).
        if (resources->wicSource == nullptr)
        {
            ComPtr<IWICBitmapDecoder> decoder;
            HRESULT hr = wicFactory->CreateDecoderFromFilename(
                resources->imageFilename.c_str(),
                nullptr,
                GENERIC_READ,
                WICDecodeMetadataCacheOnDemand,
                &decoder);

            if FAILED(hr)
            {
                if (HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) == hr)
                {
                    resources->imageIsValid = false;
                    return;
                }
                else
                {
                    DX::ThrowIfFailed(hr);
                }
            }

            ComPtr<IWICBitmapFrameDecode> frame;
            DX::ThrowIfFailed(decoder->GetFrame(0, &frame));

            // Always convert to FP16 for JXR support. We ignore color profiles in this tool.
            WICPixelFormatGUID outFmt = GUID_WICPixelFormat64bppPRGBAHalf;

            ComPtr<IWICFormatConverter> converter;
            DX::ThrowIfFailed(wicFactory->CreateFormatConverter(&converter));
            DX::ThrowIfFailed(converter->Initialize(
                frame.Get(),
                outFmt,
                WICBitmapDitherTypeNone,
                nullptr,
                0.0f,
                WICBitmapPaletteTypeCustom));

            DX::ThrowIfFailed(converter.As(&resources->wicSource));
        }

        // Next, ensure that there is a D2D source (device dependent).
        if (resources->d2dSource == nullptr)
        {
            assert(resources->wicSource != nullptr);

            DX::ThrowIfFailed(ctx->CreateImageSourceFromWic(resources->wicSource.Get(), &resources->d2dSource));

            // The image is only valid if both WIC and D2D resources are ready.
            resources->imageIsValid = true;
        }
    }
}

void Game::LoadEffectResources(TestPatternResources* resources)
{
    auto ctx = m_deviceResources->GetD2DDeviceContext();

    // This test involves a shader file.
    if (resources->effectShaderFilename.compare(L"") != 0)
    {
        assert(resources->effectClsid != GUID{});

        try
        {
            DX::ThrowIfFailed(ctx->CreateEffect(resources->effectClsid, &resources->d2dEffect));
            resources->effectIsValid = true;
        }
        catch (std::exception)
        {
            // Most likely caused by a missing cso file. Continue on.
            resources->effectIsValid = false;
        }
    }
}

void Game::OnDeviceLost()
{
    m_gradientBrush.Reset();
    m_testTitleLayout.Reset();
    m_panelInfoTextLayout.Reset();
    m_panelInfoTextLayout.Reset();
    m_whiteBrush.Reset();

    for (auto it = m_testPatternResources.begin(); it != m_testPatternResources.end(); it++)
    {
        // Only invalidate the device dependent resources.
        it->second.d2dSource.Reset();
        it->second.imageIsValid = false;
        it->second.d2dEffect.Reset();
        it->second.effectIsValid = false;
    }
}

void Game::OnDeviceRestored()
{
    CreateDeviceDependentResources();

    CreateWindowSizeDependentResources();
}
#pragma endregion

#pragma region Test pattern control
// Set increment = true to go up, false to go down.
void Game::SetTestPattern(TestPattern testPattern)
{
    // save previous pattern in cache
    if (testPattern == TestPattern::Cooldown
        && m_currentTest != TestPattern::Cooldown)
        m_cachedTest = m_currentTest;

    if (TestPattern::StartOfTest <= testPattern)
    {

        if (TestPattern::Cooldown >= testPattern)
        {
            m_currentTest = testPattern;
        }
    }

    //	m_showExplanatoryText = true;
    m_newTestSelected = true;
}

// Set increment = true to go up, false to go down.
void Game::ChangeTestPattern(bool increment)
{
    if (TestPattern::Cooldown == m_currentTest)
    {
        m_currentTest = m_cachedTest;
		m_newTestSelected = true;
        return;
    }

    if (increment)
    {
        if (TestPattern::EndOfTest == m_currentTest)
        {
            // Clamp to the end of the list.
        }
        else
        {
            unsigned int testInt = static_cast<unsigned int>(m_currentTest) + 1;
            m_currentTest = static_cast<TestPattern>(testInt);
        }
    }
    else
    {
        if (TestPattern::StartOfTest == m_currentTest)
        {
            // Clamp to the start of the list.
        }
        else
        {
            unsigned int testInt = static_cast<unsigned int>(m_currentTest) - 1;
            m_currentTest = static_cast<TestPattern>(testInt);
        }
    }

    //	m_showExplanatoryText = true;
    m_newTestSelected = true;
}

void Game::SetShift(bool shift)
{
	m_shiftKey = shift;
}

void Game::ChangeSubtest( INT32 increment )		// called from up/down arrow keys (at least)
{
	if (m_shiftKey)
		increment *= 10;

	switch (m_currentTest)
	{
	case TestPattern::PanelCharacteristics:
	case TestPattern::TenPercentPeakMAX:
	case TestPattern::TenPercentPeak:
//	case TestPattern::ProfileCurve:
//	case TestPattern::SubTitleFlicker:
//	case TestPattern::XRiteColors:
		int testTier;
		testTier = (int)m_testingTier;
		testTier += increment;
		testTier = clamp(testTier, (int)TestingTier::DisplayHDR400, (int)TestingTier::DisplayHDR10000);
		m_testingTier = (TestingTier)testTier;
		break;

	case TestPattern::StaticContrastRatio:
		if (CheckHDR_On())
		{
			m_staticContrastPQValue += increment;
			m_staticContrastPQValue = clamp(m_staticContrastPQValue, 100, 750);		// 1..?? nits
		}
		else
		{
			m_staticContrastsRGBValue += increment;
			m_staticContrastsRGBValue = clamp(m_staticContrastsRGBValue, 0.f, 255.f);
		}
		break;

	case TestPattern::ActiveDimming:
		m_activeDimming50PQValue += increment;
		m_activeDimming50PQValue = clamp(m_activeDimming50PQValue, 420, 488);	// 35..75 nits
		break;

	case TestPattern::ActiveDimmingDark:
		m_activeDimming05PQValue += increment;
		m_activeDimming05PQValue = clamp(m_activeDimming05PQValue, 208, 292); 	// 2.5..8 nits
		break;

	case TestPattern::CalibrateMaxEffectiveValue:
		if (CheckHDR_On())
		{
			m_maxEffectivePQValue += increment;
			m_maxEffectivePQValue = clamp(m_maxEffectivePQValue, 0.0f, 10000.0f);
		}
		else
		{
			m_maxEffectivesRGBValue += increment;
			m_maxEffectivesRGBValue = clamp(m_maxEffectivesRGBValue, 0.0f, 255.0f);
		}
		break;

	case TestPattern::CalibrateMaxEffectiveFullFrameValue:
		if (CheckHDR_On())
		{
			m_maxFullFramePQValue += increment;
			m_maxFullFramePQValue = clamp(m_maxFullFramePQValue, 0.0f, 10000.0f);
		}
		else
		{
			m_maxFullFramesRGBValue += increment;
			m_maxFullFramesRGBValue = clamp(m_maxFullFramesRGBValue, 0.0f, 255.0f);
		}
		break;

	case TestPattern::CalibrateMinEffectiveValue:
		if (CheckHDR_On())
		{
			m_minEffectivePQValue += increment;
			m_minEffectivePQValue = clamp(m_minEffectivePQValue, 0.0f, 10000.0f);
		}
		else
		{
			m_minEffectivesRGBValue += increment;
			m_minEffectivesRGBValue = clamp(m_minEffectivesRGBValue, 0.0f, 255.0f);
		}
		break;

	// These all rotate among R, G, B, and W.
	case TestPattern::ColorPatches:						// square patch in center			
	case TestPattern::ColorPatchesFull:					// for fullscreen case
	case TestPattern::ColorPatchesMAX:					// legacy
	case TestPattern::ColorPatches709:
	case TestPattern::FullFrameSDRWhite:
	case TestPattern::FullFrameSDRWhiteWithHDR:
	case TestPattern::SharpeningFilter:
	case TestPattern::ToneMapSpike:
		m_currentColor -= increment;				
		m_currentColor = (int) wrap( (float)m_currentColor, 0.f, 3.f );	// Just wrap on each end
		break;

	case TestPattern::ProfileCurve:
		m_currentProfileTile += increment;
		m_currentProfileTile = (int) wrap((float)m_currentProfileTile, 0.f, (float)m_maxProfileTile);
		break;

	// 5 new tests for v1.2								// TODO
	case TestPattern::LocalDimmingContrast:				// swtich white bars based on tier
		m_LocalDimmingBars -= increment;
		m_LocalDimmingBars = (int) wrap((float)m_LocalDimmingBars, 0.f, 1.f);	// Just wrap on each end <inclusive!>
		break;

	case TestPattern::BlackLevelHDRvsSDR:				// No UI change - just be clear on SDR vs HDR in text
		break;

	case TestPattern::BlackLevelCrush:					// rotate through 4 fulllscreen background colors
		m_currentBlack -= increment;
		m_currentBlack = (int) wrap((float)m_currentBlack, 0.f, 4.f);	// Just wrap on each end <inclusive!>
		break;

	case TestPattern::XRiteColors:						// cycle through the official Xrite patch colors
		if (m_XRitePatchAutoMode)
			break;										// ignore arrow keys if auto advancing

		m_currentXRiteIndex -= increment;
		m_currentXRiteIndex = (int)wrap((float)m_currentXRiteIndex, 0.f, NUMXRITECOLORS );	// Just wrap on each end <inclusive!>
		break;

	case TestPattern::StaticGradient:
		if ( increment < 0 )
			ChangeGradientColor(-0.05f, -0.05f, -0.05f);
		else
			ChangeGradientColor(+0.05f, +0.05f, +0.05f);

	}
}

void Game::ChangeGradientColor(float deltaR, float deltaG, float deltaB)
{
    m_gradientColor.r += deltaR;
    m_gradientColor.g += deltaG;
    m_gradientColor.b += deltaB;
}

void Game:: ChangeCheckerboard(bool increment)
{
	if (increment)
	{
		if (Checkerboard::Cb4x3not == m_checkerboard )
		{
			// Wrap to the start of the list.
			m_checkerboard = Checkerboard::Cb6x4;
		}
		else
		{
			unsigned int checkInt = static_cast<unsigned int>(m_checkerboard) + 1;
			m_checkerboard = static_cast<Checkerboard>(checkInt);
		}
	}
	else
	{
		if (Checkerboard::Cb6x4 == m_checkerboard)
		{
			// Wrap to the end of the list.
			m_checkerboard = Checkerboard::Cb4x3not;
		}
		else
		{
			unsigned int checkInt = static_cast<unsigned int>(m_checkerboard) - 1;
			m_checkerboard = static_cast<Checkerboard>(checkInt);
		}
	}
}

void Game::StartTestPattern(void)
{
    m_currentTest = TestPattern::StartOfTest;
    // m_showExplanatoryText = true;
}

// TODO: Currently unused, but kept in case we want to emulate 8 bit behavior.
void Game::ChangeBackBufferFormat(DXGI_FORMAT fmt)
{
    // Just pass the new state to DeviceResources. It will use IDeviceNotify to ensure
    // that Game also recreates its resources.
    m_deviceResources->ChangeBackBufferFormat(fmt);
}

// Many test patterns can be affected by the presence of explanatory text.
// Allow user to toggle visibility on/off for each test pattern.
// Returns whether the visibility is true or false after the update.
bool Game::ToggleInfoTextVisible()
{
    m_showExplanatoryText = !m_showExplanatoryText;
    return m_showExplanatoryText;
}

bool Game::ToggleSubtitle()
{
	m_subtitleVisible = !m_subtitleVisible;
	return m_subtitleVisible;
}

void Game::ToggleXRitePatchAuto( void )
{
	m_XRitePatchAutoMode = !m_XRitePatchAutoMode;

	if (m_XRitePatchAutoMode)				// reset counter on mode start
	{
		m_currentXRiteIndex = 0;
		m_testTimeRemainingSec = m_XRitePatchDisplayTime;
	}
}

// used to manually correct for monitor not being exactly PQ/2084 profile
void Game::TweakBrightnessBias(INT32 increment)
{
	if (m_currentTest == TestPattern::XRiteColors)
	{
		m_XRitePatchDisplayTime += increment * 0.1f;
		return;
	}

	if (m_shiftKey)
		increment *= 10;

	m_brightnessBias += increment;
}

// select brightness level for color patch test - usually based on target testing tier
void Game::SelectWhiteLevel(INT32 increment)
{
	if (increment > 0)
	{
		m_whiteLevelBracket++;
		if (m_whiteLevelBracket > NUM_WBRACKETS-1)
			m_whiteLevelBracket = 0;					// wrap
	}

	if (increment < 0)
	{
		m_whiteLevelBracket--;
		if (m_whiteLevelBracket < 0)
			m_whiteLevelBracket = NUM_WBRACKETS - 1;	// wrap
	}
}


#pragma endregion


#if 0
// computes percentage tri1/tri2: inputs are in xy, math and output are in uv coords
float Game::ComputeGamutCoverage(float2 r1, float2 g1, float2 b1, float2 r2, float2 g2, float2 b2)
{
	// Use D2D to compute triangle intersection area
	float2 red_uv, grn_uv, blu_uv;
	D2D_POINT_2F red_pt, grn_pt, blu_pt;
	auto fact = m_deviceResources->GetD2DFactory();

	// Triangle1
	// Convert from 1931 xy to 1976 uv coordinates:
	const float DIPSCALE = 0.1f;
	red_uv = xytouv(r1)*DIPSCALE;
	grn_uv = xytouv(g1)*DIPSCALE;
	blu_uv = xytouv(b1)*DIPSCALE;

	// put into D2D format
	red_pt = D2D1::Point2F(red_uv.x, red_uv.y);
	grn_pt = D2D1::Point2F(grn_uv.x, grn_uv.y);
	blu_pt = D2D1::Point2F(blu_uv.x, blu_uv.y);

	// Create a path for the first gamut triangle
	ComPtr<ID2D1PathGeometry> pGamutTriangle1;
	DX::ThrowIfFailed(fact->CreatePathGeometry(&pGamutTriangle1));

	ComPtr<ID2D1GeometrySink> pGamutTriangleSink;
	DX::ThrowIfFailed(pGamutTriangle1->Open(&pGamutTriangleSink));
	pGamutTriangleSink->BeginFigure(red_pt, D2D1_FIGURE_BEGIN_FILLED);
	pGamutTriangleSink->AddLine(grn_pt);
	pGamutTriangleSink->AddLine(blu_pt);
	pGamutTriangleSink->EndFigure(D2D1_FIGURE_END_CLOSED);
	DX::ThrowIfFailed(pGamutTriangleSink->Close());

	// Triangle2
	// Convert from 1931 xy to 1976 uv coordinates:
	red_uv = xytouv(r2)*DIPSCALE;
	grn_uv = xytouv(g2)*DIPSCALE;
	blu_uv = xytouv(b2)*DIPSCALE;

	// put into D2D format
	red_pt = D2D1::Point2F(red_uv.x, red_uv.y);
	grn_pt = D2D1::Point2F(grn_uv.x, grn_uv.y);
	blu_pt = D2D1::Point2F(blu_uv.x, blu_uv.y);

	// Create a path for the second gamut triangle
	ComPtr<ID2D1PathGeometry> pGamutTriangle2;
	DX::ThrowIfFailed(fact->CreatePathGeometry(&pGamutTriangle2));

	DX::ThrowIfFailed(pGamutTriangle2->Open(&pGamutTriangleSink));
	pGamutTriangleSink->BeginFigure(red_pt, D2D1_FIGURE_BEGIN_FILLED);
	pGamutTriangleSink->AddLine(grn_pt);
	pGamutTriangleSink->AddLine(blu_pt);
	pGamutTriangleSink->EndFigure(D2D1_FIGURE_END_CLOSED);
	DX::ThrowIfFailed(pGamutTriangleSink->Close());

	// now compute intersection
	HRESULT hr;

	ComPtr<ID2D1PathGeometry> pPathGeometryIntersect;
	hr = fact->CreatePathGeometry(&pPathGeometryIntersect);

	ComPtr<ID2D1GeometrySink> pGeometrySink;
	hr = pPathGeometryIntersect->Open(&pGeometrySink);

	hr = pGamutTriangle1.Get()->CombineWithGeometry(pGamutTriangle2.Get(), D2D1_COMBINE_MODE_INTERSECT,
		NULL, NULL,
		pGeometrySink.Get()
	);

	hr = pGeometrySink->Close();

	float gamutArea;
	pPathGeometryIntersect->ComputeArea(D2D1::IdentityMatrix(), &gamutArea);

	return gamutArea / ComputeGamutArea(r2, g2, b2) / DIPSCALE / DIPSCALE;

}
#endif
