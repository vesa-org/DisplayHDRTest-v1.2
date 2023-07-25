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

#pragma once

#include "DeviceResources.h"
#include "StepTimer.h"
#include "Basicmath.h"
#include <map>

#include <winrt\Windows.Devices.Display.h>
#include <winrt\Windows.Devices.Display.Core.h>
#include <winrt\Windows.Devices.Enumeration.h>

struct rawOutputDesc
{
	float MaxLuminance;
	float MaxFullFrameLuminance;
	float MinLuminance;
};

// A basic game implementation that creates a D3D11 device and
// provides a game loop.
class Game : public DX::IDeviceNotify
{
private:
    enum TestingTier
    {
        DisplayHDR400   = 0,
		DisplayHDR500   = 1,
		DisplayHDR600   = 2,
        DisplayHDR1000  = 3,
        DisplayHDR1400  = 4,
		DisplayHDR2000  = 5,
		DisplayHDR3000  = 6,
        DisplayHDR4000  = 7,
        DisplayHDR6000  = 8,
        DisplayHDR10000 = 9
    };

    enum ColorGamut
    {
        GAMUT_Native = 0,
        GAMUT_sRGB   = 1,
        GAMUT_Adobe  = 2,
        GAMUT_DCIP3  = 3,
        GAMUT_BT2100 = 4,
        GAMUT_ACES   = 5,
    };

    #define NUM_WBRACKETS 8
    UINT32 WhiteLevelBrackets[NUM_WBRACKETS] = { 50, 100, 200, 250, 300, 500, 700, 1000 };

    // Used by any test that loads a custom effect or image from file.
    struct TestPatternResources
    {
        std::wstring testTitle; // Mandatory.
        std::wstring imageFilename; // Empty means no image is needed for this test.
        std::wstring effectShaderFilename; // Empty means no shader is needed for this test.
        GUID effectClsid;
        // Members above this point need to be specified at app start.
        // ---
        // Members below this point are generated dynamically.
        Microsoft::WRL::ComPtr<IWICBitmapSource> wicSource; // Generated from WIC.
        Microsoft::WRL::ComPtr<ID2D1ImageSourceFromWic> d2dSource; // Generated from D2D.
        Microsoft::WRL::ComPtr<ID2D1Effect> d2dEffect; // Generated from D2D.
        bool imageIsValid; // false means image file is missing or invalid.
        bool effectIsValid; // false means effect file is missing or invalid.
    };

public:

    Game(PWSTR appTitle);

    enum class Checkerboard             // Which pattern to use in Checkerboard tests                    5.x
    {
        Cb6x4,
        Cb4x3,
        Cb4x3not,
    };

	enum class TestPattern
	{
		StartOfTest, // Must always be first.
		ConnectionProperties,
		PanelCharacteristics,
		ResetInstructions,
        PQLevelsInNits,
        WarmUp,
        TenPercentPeak,             // 1.
        TenPercentPeakMAX,          // 1. MAX
        FlashTest,                  // 2. 
        FlashTestMAX,               // 2. MAX
        LongDurationWhite,          // 3.
        FullFramePeak,              // 3. MAX
		DualCornerBox,				// 4. Total contrast test for TrueBlack
		StaticContrastRatio,		// 5. was tunnel
		ActiveDimming,				// 5.1
		ActiveDimmingDark,			// 5.2
		ActiveDimmingSplit,			// 5.3
		ColorPatches,               // 6. 8-10% OPR
		ColorPatchesFull,           // 6. Fullscreen 100% OPR
		BitDepthPrecision,          // 7. Uses custom effect.
        RiseFallTime,               // 8.
		ProfileCurve,               // 9. Validate tracking of 2084 profile
        LocalDimmingContrast,       // v1.2 
        BlackLevelHDRvsSDR,         // v1.2 
        BlackLevelCrush,            // v1.2
        SubTitleFlicker,            // v1.2 
        XRiteColors,			    // v1.2 cycle through the official Xrite patch colors
		EndOfMandatoryTests,        // 
        SharpeningFilter,           // Uses custom Sine Sweep effect.
		ToneMapSpike,				// Uses custom Tone Spike effect
        TextQuality,                // Uses image.
        //PQLevelsInNitsDynamic,
        OnePixelLinesBW,            // Uses image.
        OnePixelLinesRG,            // Uses image.
        ColorPatches709,
        FullFrameSDRWhite,
        FullFrameSDRWhiteWithHDR,
		CalibrateMaxEffectiveValue,		// max value to send that has any effect
		CalibrateMaxEffectiveFullFrameValue,	// MaxFALL after tone mapping
		CalibrateMinEffectiveValue,		// minimal value that is still visible by user
        StaticGradient,
        AnimatedGrayGradient,
        AnimatedColorGradient,
		BlackLevelHdrCorners,       // 4.
		BlackLevelSdrTunnel,        // 5. Tunnel test Moved here from v1.0. Replaced with StaticContrastRatio in v1.1
		ColorPatchesMAX,            // 6. MAX
		EndOfTest, // Must always be last.
        Cooldown,
    };

    // Initialization and management
    void Initialize(HWND window, int width, int height);

    // Basic game loop
    void Tick();

    // IDeviceNotify
    virtual void OnDeviceLost() override;
    virtual void OnDeviceRestored() override;

    // Messages
    void OnActivated();
    void OnDeactivated();
    void OnSuspending();
    void OnResuming();
    void OnWindowSizeChanged(int width, int height);
    void OnDisplayChange();

    // Properties
    void GetDefaultSize(int& width, int& height) const;

    // Test pattern control
    void SetTestPattern(TestPattern testPattern);
    void ChangeTestPattern(bool increment);
    void ChangeSubtest( INT32 increment );
    void SetShift( bool shift );
    void ToggleXRitePatchAuto( void );
    void TweakBrightnessBias(INT32 increment);
    void SelectWhiteLevel(INT32 incrememnt);
    void StartTestPattern(void);
    void ChangeGradientColor(float deltaR, float deltaG, float deltaB);
    void ChangeBackBufferFormat(DXGI_FORMAT fmt);
    void ChangeCheckerboard(bool increment);
    bool ToggleInfoTextVisible();
    void SetMetadataNeutral(); // OS defaults
	void PrintMetadata( ID2D1DeviceContext2* ctx, bool blackText = false );

private:

    void ConstructorInternal();

    void Update(DX::StepTimer const& timer);
    void UpdateDxgiColorimetryInfo();
	void InitEffectiveValues();
    void SetMetadata(float max, float avg, ColorGamut gamut);
    void Render();
	bool CheckHDR_On();
    bool CheckForDefaults();
	void DrawLogo(ID2D1DeviceContext2 *ctx, float c );
    void DrawChecker6x4(  ID2D1DeviceContext2* ctx, float colorL, float colorR );      // for displays > 20"
    void DrawChecker4x3(  ID2D1DeviceContext2* ctx, float colorL, float colorR );      // for smaller panels
    void DrawChecker4x3n( ID2D1DeviceContext2* ctx, float colorL, float colorR );      // nverse of above
    TestingTier GetTestingTier();
    WCHAR *GetTierName(TestingTier tier);
	float GetTierLuminance(Game::TestingTier tier);

    // float ComputeGamutArea( float2 r, float2 g, float2 b );
    // float ComputeGamutCoverage( float2 r1, float2 g1, float2 b1, float2 r2, float2 g2, float2 b2 );

    // Drawing code specific for each test pattern.
    void GenerateTestPattern_StartOfTest(ID2D1DeviceContext2* ctx);
    void GenerateTestPattern_ConnectionProperties(ID2D1DeviceContext2* ctx);
    void GenerateTestPattern_PanelCharacteristics(ID2D1DeviceContext2* ctx);
    void GenerateTestPattern_ResetInstructions(ID2D1DeviceContext2* ctx);
	void GenerateTestPattern_CalibrateMaxEffectiveValue(ID2D1DeviceContext2* ctx);
	void GenerateTestPattern_CalibrateMaxFullFrameValue(ID2D1DeviceContext2* ctx);
	void GenerateTestPattern_CalibrateMinEffectiveValue(ID2D1DeviceContext2* ctx);
	void GenerateTestPattern_PQLevelsInNits(ID2D1DeviceContext2* ctx);
    void GenerateTestPattern_WarmUp(ID2D1DeviceContext2* ctx);
    void GenerateTestPattern_Cooldown(ID2D1DeviceContext2* ctx);
    void GenerateTestPattern_TenPercentPeak(ID2D1DeviceContext2* ctx);						// 1
    void GenerateTestPattern_TenPercentPeakMAX(ID2D1DeviceContext2* ctx);					// 1.0b MAX
    void GenerateTestPattern_FlashTest(ID2D1DeviceContext2* ctx);							// 2
    void GenerateTestPattern_FlashTestMAX(ID2D1DeviceContext2* ctx);						// 2. Max
	void GenerateTestPattern_LongDurationWhite(ID2D1DeviceContext2* ctx);					// 3
	void GenerateTestPattern_DualCornerBox(ID2D1DeviceContext2* ctx);						// 4
	void GenerateTestPattern_StaticContrastRatio(ID2D1DeviceContext2 * ctx);				// 5
	void GenerateTestPattern_ActiveDimming(ID2D1DeviceContext2 * ctx);						// 5.1
	void GenerateTestPattern_ActiveDimmingDark(ID2D1DeviceContext2 * ctx);					// 5.2
	void GenerateTestPattern_ActiveDimmingSplit(ID2D1DeviceContext2 * ctx);					// 5.3
    void GenerateTestPattern_ColorPatches(   ID2D1DeviceContext2* ctx, bool full = false );	// 6
    void GenerateTestPattern_ColorPatchesMAX(ID2D1DeviceContext2* ctx, float OPR );			// 6.b MAX legacy
    void GenerateTestPattern_BitDepthPrecision(ID2D1DeviceContext2* ctx);					// 7
	void GenerateTestPattern_RiseFallTime(ID2D1DeviceContext2* ctx);						// 8
	void GenerateTestPattern_ProfileCurve(ID2D1DeviceContext2* ctx);						// 9
    void GenerateTestPattern_LocalDimmingContrast(ID2D1DeviceContext2* ctx);				// v1.2
    void GenerateTestPattern_BlackLevelHDRvsSDR(ID2D1DeviceContext2* ctx);		    		// v1.2
    void GenerateTestPattern_BlackLevelCrush(ID2D1DeviceContext2* ctx);		        		// v1.2
    void GenerateTestPattern_SubTitleFlicker(ID2D1DeviceContext2* ctx);	        			// v1.2
	void GenerateTestPattern_XRiteColors(ID2D1DeviceContext2* ctx);                         // v1.2
    void GenerateTestPattern_EndOfMandatoryTests(ID2D1DeviceContext2* ctx);
    void GenerateTestPattern_FullFramePeak(ID2D1DeviceContext2* ctx);						// 3.b MAX legacy
	void GenerateTestPattern_BlackLevelHdrCorners(ID2D1DeviceContext2* ctx);				// 4 legacy
	void GenerateTestPattern_SharpeningFilter(ID2D1DeviceContext2* ctx);
	void GenerateTestPattern_ToneMapSpike(ID2D1DeviceContext2* ctx);
	void GenerateTestPattern_FullFrameSDRWhite(ID2D1DeviceContext2* ctx);
    void GenerateTestPattern_FullFrameSDRWhiteWithHDR(ID2D1DeviceContext2* ctx);
    void GenerateTestPattern_ColorPatches709(ID2D1DeviceContext2 * ctx);
    void GenerateTestPattern_StaticGradient(ID2D1DeviceContext2* ctx);
    void GenerateTestPattern_AnimatedGrayGradient(ID2D1DeviceContext2* ctx);
    void GenerateTestPattern_AnimatedColorGradient(ID2D1DeviceContext2* ctx);
	void GenerateTestPattern_BlackLevelSdrTunnel(ID2D1DeviceContext2* ctx);
	void GenerateTestPattern_BlackLevelSdrTunnelTrueBlack(ID2D1DeviceContext2* ctx);
	void GenerateTestPattern_EndOfTest(ID2D1DeviceContext2* ctx);

    // Generalized routine for all tests that involve loading an image.
    void GenerateTestPattern_ImageCommon(ID2D1DeviceContext2* ctx, TestPatternResources resources);

    // Common rendering subroutines.
    void Clear();
    void RenderText(ID2D1DeviceContext2* ctx, IDWriteTextFormat* fmt, std::wstring text, D2D1_RECT_F textPos, bool useBlackText = false);

    void CreateDeviceIndependentResources();
    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources();
    void LoadTestPatternResources(TestPatternResources* resources);
    void LoadImageResources(TestPatternResources* resources);
    void LoadEffectResources(TestPatternResources* resources);

    // Device resources.
    std::unique_ptr<DX::DeviceResources>    m_deviceResources;
    DXGI_ADAPTER_DESC                                       m_adapterDesc;

    winrt::hstring                                          m_monitorName;                  // friendlier name
    winrt::Windows::Devices::Display::DisplayMonitorConnectionKind        m_connectionKind;               // Internal vs wired
    winrt::Windows::Devices::Display::DisplayMonitorPhysicalConnectorKind m_physicalConnectorKind;        // HDMI vs DisplayPort
    winrt::Windows::Devices::Display::DisplayMonitorDescriptorKind        m_connectionDescriptorKind;     // EDID vs DisplayID

    DXGI_RATIONAL                                           m_verticalSyncRate;     // Current mode's max rate
    DWORD                                                   m_displayFrequency;     // from EnumDisplaySettings -not precise


    Microsoft::WRL::ComPtr<ID2D1LinearGradientBrush>        m_gradientBrush;
    Microsoft::WRL::ComPtr<IDWriteTextLayout>               m_testTitleLayout;
    Microsoft::WRL::ComPtr<IDWriteTextLayout>               m_panelInfoTextLayout;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush>            m_whiteBrush;
	Microsoft::WRL::ComPtr<ID2D1SolidColorBrush>            m_blackBrush;
	Microsoft::WRL::ComPtr<ID2D1SolidColorBrush>            m_redBrush;

    DXGI_OUTPUT_DESC1                                       m_outputDesc;
	rawOutputDesc											m_rawOutDesc;		// base values from OS before scaling due to brightness setting

    // Device independent resources.
    Microsoft::WRL::ComPtr<IDWriteTextFormat>               m_smallFormat;
    Microsoft::WRL::ComPtr<IDWriteTextFormat>               m_largeFormat;
    Microsoft::WRL::ComPtr<IDWriteTextFormat>               m_monospaceFormat;
    D2D1_RECT_F                                             m_testTitleRect;    // Where to draw each test's title
    D2D1_RECT_F                                             m_largeTextRect;    // Where to draw large text for the test, if applicable
	D2D1_RECT_F												m_MetadataTextRect;
    TestingTier                                             m_testingTier;
    TestPattern                                             m_currentTest;
    TestPattern                                             m_cachedTest;
    bool                                                    m_shiftKey;         // whether shift key is pressed
    INT32                                                   m_currentColor;
    INT32                                                   m_currentBlack;     // for black crush test
    INT32													m_currentProfileTile;
    INT32                                                   m_modeWidth;        // resolution of current mode (actually native res now)
    INT32                                                   m_modeHeight;
    float                                                   m_snoodDiam;        // outside diameter of sensor snood in mm

	UINT32													m_maxPQCode;		// PQ code of maxLuminance
	INT32													m_maxProfileTile;	// highest tile worth testing on this panel
    float                                                   m_flashOn;
    Checkerboard                                            m_checkerboard;     // for tests        5.x
    D2D1_COLOR_F                                            m_gradientColor;
    INT32                                                   m_LocalDimmingBars;                 // v1.2 Local Dimming Contrast Test
    INT32                                                   m_subTitleVisible;                  // v1.4 subTitle Flicker Test
    INT32                                                   m_currentXRiteIndex;                // v1.5 XRite Color Patch
    bool                                                    m_XRitePatchAutoMode;               // v1.5 flag for when it auto animates
    float                                                   m_XRitePatchDisplayTime;            // how long to show XRite color patch in auto mode
//  float                                                   m_XRitePatchTimer;                  // timer for tracking above
    INT32                                                   m_whiteLevelBracket;                // what tier/level we should use
    float                                                   m_brightnessBias;                   // correction for panel
    float                                                   m_gradientAnimationBase;
    bool                                                    m_showExplanatoryText;
    float                                                   m_testTimeRemainingSec;
    float                                                   m_gamutVolume;
//TODO: these code values could all be INTs
	float													m_maxEffectivesRGBValue;		    // Code levels via manual test
	float													m_maxFullFramesRGBValue;
	float													m_minEffectivesRGBValue;
	float													m_maxEffectivePQValue;	            // PQ is using HDR10 10bit encoding
	float													m_maxFullFramePQValue;
	float													m_minEffectivePQValue;
    float                                                   m_staticContrastPQValue;            // for test 5. static contrast
    float                                                   m_staticContrastsRGBValue;
    float													m_activeDimming50PQValue;           // for tests 5.1, 5.2, 5.3 on Active Dimming
	float													m_activeDimming05PQValue;
    float													m_activeDimming50sRGBValue;         // probably no active dimming in sRGB mode
    float													m_activeDimming05sRGBValue;         // only in HDR mode

	bool                                                    m_newTestSelected; // Used for one-time initialization of test variables.
    bool                                                    m_dxgiColorInfoStale;
	DXGI_HDR_METADATA_HDR10									m_Metadata;
	ColorGamut												m_MetadataGamut;


    // TODO: integrate this with the other test resources
    std::map<TestPattern, TestPatternResources>             m_testPatternResources;
    std::wstring                                            m_hideTextString;

    // Rendering loop timer.
    DX::StepTimer                           m_timer;
    float                                   m_totalTime;

    PWSTR                                   m_appTitle;
};
