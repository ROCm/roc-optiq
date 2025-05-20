// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.
// Simple wrapper around Adobe's Spectrum fork of ImGUI providing dynamic switching between Light & Dark themes.

#include "imgui_spectrum_dynamic.h"

#include "imgui_spectrum.cpp"
#include "imgui_widgets.cpp"

#undef NONE
#undef WHITE
#undef BLACK
#undef GRAY50
#undef GRAY75
#undef GRAY100
#undef GRAY200
#undef GRAY300
#undef GRAY400
#undef GRAY500
#undef GRAY600
#undef GRAY700
#undef GRAY800
#undef GRAY900
#undef BLUE400
#undef BLUE500
#undef BLUE600
#undef BLUE700
#undef RED400
#undef RED500
#undef RED600
#undef RED700
#undef ORANGE400
#undef ORANGE500
#undef ORANGE600
#undef ORANGE700
#undef GREEN400
#undef GREEN500
#undef GREEN600
#undef GREEN700
#undef INDIGO400
#undef INDIGO500
#undef INDIGO600
#undef INDIGO700
#undef CELERY400
#undef CELERY500
#undef CELERY600
#undef CELERY700
#undef MAGENTA400
#undef MAGENTA500
#undef MAGENTA600
#undef MAGENTA700
#undef YELLOW400
#undef YELLOW500
#undef YELLOW600
#undef YELLOW700
#undef FUCHSIA400
#undef FUCHSIA500
#undef FUCHSIA600
#undef FUCHSIA700
#undef SEAFOAM400
#undef SEAFOAM500
#undef SEAFOAM600
#undef SEAFOAM700
#undef CHARTREUSE400
#undef CHARTREUSE500
#undef CHARTREUSE600
#undef CHARTREUSE700
#undef PURPLE400
#undef PURPLE500
#undef PURPLE600
#undef PURPLE700

namespace ImGui {
    namespace Spectrum {
        namespace Static {
            unsigned int GetColor(ColorEnum color)
            {
                switch (color)
                {
                    case ColorEnum::NONE: return ImGui::Spectrum::Static::NONE;
                    case ColorEnum::WHITE: return ImGui::Spectrum::Static::WHITE;
                    case ColorEnum::BLACK: return ImGui::Spectrum::Static::BLACK;
                    case ColorEnum::GRAY200: return ImGui::Spectrum::Static::GRAY200;
                    case ColorEnum::GRAY300: return ImGui::Spectrum::Static::GRAY300;
                    case ColorEnum::GRAY400: return ImGui::Spectrum::Static::GRAY400;
                    case ColorEnum::GRAY500: return ImGui::Spectrum::Static::GRAY500;
                    case ColorEnum::GRAY600: return ImGui::Spectrum::Static::GRAY600;
                    case ColorEnum::GRAY700: return ImGui::Spectrum::Static::GRAY700;
                    case ColorEnum::GRAY800: return ImGui::Spectrum::Static::GRAY800;
                    case ColorEnum::GRAY900: return ImGui::Spectrum::Static::GRAY900;
                    case ColorEnum::BLUE400: return ImGui::Spectrum::Static::BLUE400;
                    case ColorEnum::BLUE500: return ImGui::Spectrum::Static::BLUE500;
                    case ColorEnum::BLUE600: return ImGui::Spectrum::Static::BLUE600;
                    case ColorEnum::BLUE700: return ImGui::Spectrum::Static::BLUE700;
                    case ColorEnum::RED400: return ImGui::Spectrum::Static::RED400;
                    case ColorEnum::RED500: return ImGui::Spectrum::Static::RED500;
                    case ColorEnum::RED600: return ImGui::Spectrum::Static::RED600;
                    case ColorEnum::RED700: return ImGui::Spectrum::Static::RED700;
                    case ColorEnum::ORANGE400: return ImGui::Spectrum::Static::ORANGE400;
                    case ColorEnum::ORANGE500: return ImGui::Spectrum::Static::ORANGE500;
                    case ColorEnum::ORANGE600: return ImGui::Spectrum::Static::ORANGE600;
                    case ColorEnum::ORANGE700: return ImGui::Spectrum::Static::ORANGE700;
                    case ColorEnum::GREEN400: return ImGui::Spectrum::Static::GREEN400;
                    case ColorEnum::GREEN500: return ImGui::Spectrum::Static::GREEN500;
                    case ColorEnum::GREEN600: return ImGui::Spectrum::Static::GREEN600;
                    case ColorEnum::GREEN700: return ImGui::Spectrum::Static::GREEN700;
                }
                return 0;
            }
        }

        static bool gSpectrumLight = true;

        static const unsigned int GRAY50Light = Color(0xFFFFFF);
        static const unsigned int GRAY75Light = Color(0xFAFAFA);
        static const unsigned int GRAY100Light = Color(0xF5F5F5);
        static const unsigned int GRAY200Light = Color(0xEAEAEA);
        static const unsigned int GRAY300Light = Color(0xE1E1E1);
        static const unsigned int GRAY400Light = Color(0xCACACA);
        static const unsigned int GRAY500Light = Color(0xB3B3B3);
        static const unsigned int GRAY600Light = Color(0x8E8E8E);
        static const unsigned int GRAY700Light = Color(0x707070);
        static const unsigned int GRAY800Light = Color(0x4B4B4B);
        static const unsigned int GRAY900Light = Color(0x2C2C2C);
        static const unsigned int BLUE400Light = Color(0x2680EB);
        static const unsigned int BLUE500Light = Color(0x1473E6);
        static const unsigned int BLUE600Light = Color(0x0D66D0);
        static const unsigned int BLUE700Light = Color(0x095ABA);
        static const unsigned int RED400Light = Color(0xE34850);
        static const unsigned int RED500Light = Color(0xD7373F);
        static const unsigned int RED600Light = Color(0xC9252D);
        static const unsigned int RED700Light = Color(0xBB121A);
        static const unsigned int ORANGE400Light = Color(0xE68619);
        static const unsigned int ORANGE500Light = Color(0xDA7B11);
        static const unsigned int ORANGE600Light = Color(0xCB6F10);
        static const unsigned int ORANGE700Light = Color(0xBD640D);
        static const unsigned int GREEN400Light = Color(0x2D9D78);
        static const unsigned int GREEN500Light = Color(0x268E6C);
        static const unsigned int GREEN600Light = Color(0x12805C);
        static const unsigned int GREEN700Light = Color(0x107154);
        static const unsigned int INDIGO400Light = Color(0x6767EC);
        static const unsigned int INDIGO500Light = Color(0x5C5CE0);
        static const unsigned int INDIGO600Light = Color(0x5151D3);
        static const unsigned int INDIGO700Light = Color(0x4646C6);
        static const unsigned int CELERY400Light = Color(0x44B556);
        static const unsigned int CELERY500Light = Color(0x3DA74E);
        static const unsigned int CELERY600Light = Color(0x379947);
        static const unsigned int CELERY700Light = Color(0x318B40);
        static const unsigned int MAGENTA400Light = Color(0xD83790);
        static const unsigned int MAGENTA500Light = Color(0xCE2783);
        static const unsigned int MAGENTA600Light = Color(0xBC1C74);
        static const unsigned int MAGENTA700Light = Color(0xAE0E66);
        static const unsigned int YELLOW400Light = Color(0xDFBF00);
        static const unsigned int YELLOW500Light = Color(0xD2B200);
        static const unsigned int YELLOW600Light = Color(0xC4A600);
        static const unsigned int YELLOW700Light = Color(0xB79900);
        static const unsigned int FUCHSIA400Light = Color(0xC038CC);
        static const unsigned int FUCHSIA500Light = Color(0xB130BD);
        static const unsigned int FUCHSIA600Light = Color(0xA228AD);
        static const unsigned int FUCHSIA700Light = Color(0x93219E);
        static const unsigned int SEAFOAM400Light = Color(0x1B959A);
        static const unsigned int SEAFOAM500Light = Color(0x16878C);
        static const unsigned int SEAFOAM600Light = Color(0x0F797D);
        static const unsigned int SEAFOAM700Light = Color(0x096C6F);
        static const unsigned int CHARTREUSE400Light = Color(0x85D044);
        static const unsigned int CHARTREUSE500Light = Color(0x7CC33F);
        static const unsigned int CHARTREUSE600Light = Color(0x73B53A);
        static const unsigned int CHARTREUSE700Light = Color(0x6AA834);
        static const unsigned int PURPLE400Light = Color(0x9256D9);
        static const unsigned int PURPLE500Light = Color(0x864CCC);
        static const unsigned int PURPLE600Light = Color(0x7A42BF);
        static const unsigned int PURPLE700Light = Color(0x6F38B1);

        static const unsigned int GRAY50Dark = Color(0x252525);
        static const unsigned int GRAY75Dark = Color(0x2F2F2F);
        static const unsigned int GRAY100Dark = Color(0x323232);
        static const unsigned int GRAY200Dark = Color(0x393939);
        static const unsigned int GRAY300Dark = Color(0x3E3E3E);
        static const unsigned int GRAY400Dark = Color(0x4D4D4D);
        static const unsigned int GRAY500Dark = Color(0x5C5C5C);
        static const unsigned int GRAY600Dark = Color(0x7B7B7B);
        static const unsigned int GRAY700Dark = Color(0x999999);
        static const unsigned int GRAY800Dark = Color(0xCDCDCD);
        static const unsigned int GRAY900Dark = Color(0xFFFFFF);
        static const unsigned int BLUE400Dark = Color(0x2680EB);
        static const unsigned int BLUE500Dark = Color(0x378EF0);
        static const unsigned int BLUE600Dark = Color(0x4B9CF5);
        static const unsigned int BLUE700Dark = Color(0x5AA9FA);
        static const unsigned int RED400Dark = Color(0xE34850);
        static const unsigned int RED500Dark = Color(0xEC5B62);
        static const unsigned int RED600Dark = Color(0xF76D74);
        static const unsigned int RED700Dark = Color(0xFF7B82);
        static const unsigned int ORANGE400Dark = Color(0xE68619);
        static const unsigned int ORANGE500Dark = Color(0xF29423);
        static const unsigned int ORANGE600Dark = Color(0xF9A43F);
        static const unsigned int ORANGE700Dark = Color(0xFFB55B);
        static const unsigned int GREEN400Dark = Color(0x2D9D78);
        static const unsigned int GREEN500Dark = Color(0x33AB84);
        static const unsigned int GREEN600Dark = Color(0x39B990);
        static const unsigned int GREEN700Dark = Color(0x3FC89C);
        static const unsigned int INDIGO400Dark = Color(0x6767EC);
        static const unsigned int INDIGO500Dark = Color(0x7575F1);
        static const unsigned int INDIGO600Dark = Color(0x8282F6);
        static const unsigned int INDIGO700Dark = Color(0x9090FA);
        static const unsigned int CELERY400Dark = Color(0x44B556);
        static const unsigned int CELERY500Dark = Color(0x4BC35F);
        static const unsigned int CELERY600Dark = Color(0x51D267);
        static const unsigned int CELERY700Dark = Color(0x58E06F);
        static const unsigned int MAGENTA400Dark = Color(0xD83790);
        static const unsigned int MAGENTA500Dark = Color(0xE2499D);
        static const unsigned int MAGENTA600Dark = Color(0xEC5AAA);
        static const unsigned int MAGENTA700Dark = Color(0xF56BB7);
        static const unsigned int YELLOW400Dark = Color(0xDFBF00);
        static const unsigned int YELLOW500Dark = Color(0xEDCC00);
        static const unsigned int YELLOW600Dark = Color(0xFAD900);
        static const unsigned int YELLOW700Dark = Color(0xFFE22E);
        static const unsigned int FUCHSIA400Dark = Color(0xC038CC);
        static const unsigned int FUCHSIA500Dark = Color(0xCF3EDC);
        static const unsigned int FUCHSIA600Dark = Color(0xD951E5);
        static const unsigned int FUCHSIA700Dark = Color(0xE366EF);
        static const unsigned int SEAFOAM400Dark = Color(0x1B959A);
        static const unsigned int SEAFOAM500Dark = Color(0x20A3A8);
        static const unsigned int SEAFOAM600Dark = Color(0x23B2B8);
        static const unsigned int SEAFOAM700Dark = Color(0x26C0C7);
        static const unsigned int CHARTREUSE400Dark = Color(0x85D044);
        static const unsigned int CHARTREUSE500Dark = Color(0x8EDE49);
        static const unsigned int CHARTREUSE600Dark = Color(0x9BEC54);
        static const unsigned int CHARTREUSE700Dark = Color(0xA3F858);
        static const unsigned int PURPLE400Dark = Color(0x9256D9);
        static const unsigned int PURPLE500Dark = Color(0x9D64E1);
        static const unsigned int PURPLE600Dark = Color(0xA873E9);
        static const unsigned int PURPLE700Dark = Color(0xB483F0);

        unsigned int GetColor(ColorEnum color)
        {
            if (gSpectrumLight)
            {
                switch (color)
                {
                    case ColorEnum::GRAY50: return GRAY50Light;
                    case ColorEnum::GRAY75: return GRAY75Light;
                    case ColorEnum::GRAY100: return GRAY100Light;
                    case ColorEnum::GRAY200: return GRAY200Light;
                    case ColorEnum::GRAY300: return GRAY300Light;
                    case ColorEnum::GRAY400: return GRAY400Light;
                    case ColorEnum::GRAY500: return GRAY500Light;
                    case ColorEnum::GRAY600: return GRAY600Light;
                    case ColorEnum::GRAY700: return GRAY700Light;
                    case ColorEnum::GRAY800: return GRAY800Light;
                    case ColorEnum::GRAY900: return GRAY900Light;
                    case ColorEnum::BLUE400: return BLUE400Light;
                    case ColorEnum::BLUE500: return BLUE500Light;
                    case ColorEnum::BLUE600: return BLUE600Light;
                    case ColorEnum::BLUE700: return BLUE700Light;
                    case ColorEnum::RED400: return RED400Light;
                    case ColorEnum::RED500: return RED500Light;
                    case ColorEnum::RED600: return RED600Light;
                    case ColorEnum::RED700: return RED700Light;
                    case ColorEnum::ORANGE400: return ORANGE400Light;
                    case ColorEnum::ORANGE500: return ORANGE500Light;
                    case ColorEnum::ORANGE600: return ORANGE600Light;
                    case ColorEnum::ORANGE700: return ORANGE700Light;
                    case ColorEnum::GREEN400: return GREEN400Light;
                    case ColorEnum::GREEN500: return GREEN500Light;
                    case ColorEnum::GREEN600: return GREEN600Light;
                    case ColorEnum::GREEN700: return GREEN700Light;
                    case ColorEnum::INDIGO400: return INDIGO400Light;
                    case ColorEnum::INDIGO500: return INDIGO500Light;
                    case ColorEnum::INDIGO600: return INDIGO600Light;
                    case ColorEnum::INDIGO700: return INDIGO700Light;
                    case ColorEnum::CELERY400: return CELERY400Light;
                    case ColorEnum::CELERY500: return CELERY500Light;
                    case ColorEnum::CELERY600: return CELERY600Light;
                    case ColorEnum::CELERY700: return CELERY700Light;
                    case ColorEnum::MAGENTA400: return MAGENTA400Light;
                    case ColorEnum::MAGENTA500: return MAGENTA500Light;
                    case ColorEnum::MAGENTA600: return MAGENTA600Light;
                    case ColorEnum::MAGENTA700: return MAGENTA700Light;
                    case ColorEnum::YELLOW400: return YELLOW400Light;
                    case ColorEnum::YELLOW500: return YELLOW500Light;
                    case ColorEnum::YELLOW600: return YELLOW600Light;
                    case ColorEnum::YELLOW700: return YELLOW700Light;
                    case ColorEnum::FUCHSIA400: return FUCHSIA400Light;
                    case ColorEnum::FUCHSIA500: return FUCHSIA500Light;
                    case ColorEnum::FUCHSIA600: return FUCHSIA600Light;
                    case ColorEnum::FUCHSIA700: return FUCHSIA700Light;
                    case ColorEnum::SEAFOAM400: return SEAFOAM400Light;
                    case ColorEnum::SEAFOAM500: return SEAFOAM500Light;
                    case ColorEnum::SEAFOAM600: return SEAFOAM600Light;
                    case ColorEnum::SEAFOAM700: return SEAFOAM700Light;
                    case ColorEnum::CHARTREUSE400: return CHARTREUSE400Light;
                    case ColorEnum::CHARTREUSE500: return CHARTREUSE500Light;
                    case ColorEnum::CHARTREUSE600: return CHARTREUSE600Light;
                    case ColorEnum::CHARTREUSE700: return CHARTREUSE700Light;
                    case ColorEnum::PURPLE400: return PURPLE400Light;
                    case ColorEnum::PURPLE500: return PURPLE500Light;
                    case ColorEnum::PURPLE600: return PURPLE600Light;
                    case ColorEnum::PURPLE700: return PURPLE700Light;
                }
            }
            else
            {
                switch (color)
                {
                    case ColorEnum::GRAY50: return GRAY50Dark;
                    case ColorEnum::GRAY75: return GRAY75Dark;
                    case ColorEnum::GRAY100: return GRAY100Dark;
                    case ColorEnum::GRAY200: return GRAY200Dark;
                    case ColorEnum::GRAY300: return GRAY300Dark;
                    case ColorEnum::GRAY400: return GRAY400Dark;
                    case ColorEnum::GRAY500: return GRAY500Dark;
                    case ColorEnum::GRAY600: return GRAY600Dark;
                    case ColorEnum::GRAY700: return GRAY700Dark;
                    case ColorEnum::GRAY800: return GRAY800Dark;
                    case ColorEnum::GRAY900: return GRAY900Dark;
                    case ColorEnum::BLUE400: return BLUE400Dark;
                    case ColorEnum::BLUE500: return BLUE500Dark;
                    case ColorEnum::BLUE600: return BLUE600Dark;
                    case ColorEnum::BLUE700: return BLUE700Dark;
                    case ColorEnum::RED400: return RED400Dark;
                    case ColorEnum::RED500: return RED500Dark;
                    case ColorEnum::RED600: return RED600Dark;
                    case ColorEnum::RED700: return RED700Dark;
                    case ColorEnum::ORANGE400: return ORANGE400Dark;
                    case ColorEnum::ORANGE500: return ORANGE500Dark;
                    case ColorEnum::ORANGE600: return ORANGE600Dark;
                    case ColorEnum::ORANGE700: return ORANGE700Dark;
                    case ColorEnum::GREEN400: return GREEN400Dark;
                    case ColorEnum::GREEN500: return GREEN500Dark;
                    case ColorEnum::GREEN600: return GREEN600Dark;
                    case ColorEnum::GREEN700: return GREEN700Dark;
                    case ColorEnum::INDIGO400: return INDIGO400Dark;
                    case ColorEnum::INDIGO500: return INDIGO500Dark;
                    case ColorEnum::INDIGO600: return INDIGO600Dark;
                    case ColorEnum::INDIGO700: return INDIGO700Dark;
                    case ColorEnum::CELERY400: return CELERY400Dark;
                    case ColorEnum::CELERY500: return CELERY500Dark;
                    case ColorEnum::CELERY600: return CELERY600Dark;
                    case ColorEnum::CELERY700: return CELERY700Dark;
                    case ColorEnum::MAGENTA400: return MAGENTA400Dark;
                    case ColorEnum::MAGENTA500: return MAGENTA500Dark;
                    case ColorEnum::MAGENTA600: return MAGENTA600Dark;
                    case ColorEnum::MAGENTA700: return MAGENTA700Dark;
                    case ColorEnum::YELLOW400: return YELLOW400Dark;
                    case ColorEnum::YELLOW500: return YELLOW500Dark;
                    case ColorEnum::YELLOW600: return YELLOW600Dark;
                    case ColorEnum::YELLOW700: return YELLOW700Dark;
                    case ColorEnum::FUCHSIA400: return FUCHSIA400Dark;
                    case ColorEnum::FUCHSIA500: return FUCHSIA500Dark;
                    case ColorEnum::FUCHSIA600: return FUCHSIA600Dark;
                    case ColorEnum::FUCHSIA700: return FUCHSIA700Dark;
                    case ColorEnum::SEAFOAM400: return SEAFOAM400Dark;
                    case ColorEnum::SEAFOAM500: return SEAFOAM500Dark;
                    case ColorEnum::SEAFOAM600: return SEAFOAM600Dark;
                    case ColorEnum::SEAFOAM700: return SEAFOAM700Dark;
                    case ColorEnum::CHARTREUSE400: return CHARTREUSE400Dark;
                    case ColorEnum::CHARTREUSE500: return CHARTREUSE500Dark;
                    case ColorEnum::CHARTREUSE600: return CHARTREUSE600Dark;
                    case ColorEnum::CHARTREUSE700: return CHARTREUSE700Dark;
                    case ColorEnum::PURPLE400: return PURPLE400Dark;
                    case ColorEnum::PURPLE500: return PURPLE500Dark;
                    case ColorEnum::PURPLE600: return PURPLE600Dark;
                    case ColorEnum::PURPLE700: return PURPLE700Light;
                }
            }
            return 0;
        }

        void StyleColorsSpectrumLight()
        {
            gSpectrumLight = true;
            StyleColorsSpectrum();
        }

        void StyleColorsSpectrumDark()
        {
            gSpectrumLight = false;
            StyleColorsSpectrum();
        }
    }
}
