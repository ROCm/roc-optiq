// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.
// Simple wrapper around Adobe's Spectrum fork of ImGUI providing dynamic switching between Light & Dark themes.

#pragma once

#include "imgui_spectrum.h"

namespace ImGui {
    namespace Spectrum {
        enum class ColorEnum
        {
            NONE,
            WHITE,
            BLACK,
            GRAY50,
            GRAY75,
            GRAY100,
            GRAY200,
            GRAY300,
            GRAY400,
            GRAY500,
            GRAY600,
            GRAY700,
            GRAY800,
            GRAY900,
            BLUE400,
            BLUE500,
            BLUE600,
            BLUE700,
            RED400,
            RED500,
            RED600,
            RED700,
            ORANGE400,
            ORANGE500,
            ORANGE600,
            ORANGE700,
            GREEN400,
            GREEN500,
            GREEN600,
            GREEN700,
            INDIGO400,
            INDIGO500,
            INDIGO600,
            INDIGO700,
            CELERY400,
            CELERY500,
            CELERY600,
            CELERY700,
            MAGENTA400,
            MAGENTA500,
            MAGENTA600,
            MAGENTA700,
            YELLOW400,
            YELLOW500,
            YELLOW600,
            YELLOW700,
            FUCHSIA400,
            FUCHSIA500,
            FUCHSIA600,
            FUCHSIA700,
            SEAFOAM400,
            SEAFOAM500,
            SEAFOAM600,
            SEAFOAM700,
            CHARTREUSE400,
            CHARTREUSE500,
            CHARTREUSE600,
            CHARTREUSE700,
            PURPLE400,
            PURPLE500,
            PURPLE600,
            PURPLE700,
        };

        namespace Static {
            unsigned int GetColor(ColorEnum color);
        }
        unsigned int GetColor(ColorEnum color);

        void StyleColorsSpectrumLight(); 
        void StyleColorsSpectrumDark(); 
    }
}

#define NONE GetColor(ImGui::Spectrum::ColorEnum::NONE)
#define WHITE GetColor(ImGui::Spectrum::ColorEnum::WHITE)
#define BLACK GetColor(ImGui::Spectrum::ColorEnum::BLACK)
#define GRAY50 GetColor(ImGui::Spectrum::ColorEnum::GRAY50)
#define GRAY75 GetColor(ImGui::Spectrum::ColorEnum::GRAY75)
#define GRAY100 GetColor(ImGui::Spectrum::ColorEnum::GRAY100)
#define GRAY200 GetColor(ImGui::Spectrum::ColorEnum::GRAY200)
#define GRAY300 GetColor(ImGui::Spectrum::ColorEnum::GRAY300)
#define GRAY400 GetColor(ImGui::Spectrum::ColorEnum::GRAY400)
#define GRAY500 GetColor(ImGui::Spectrum::ColorEnum::GRAY500)
#define GRAY600 GetColor(ImGui::Spectrum::ColorEnum::GRAY600)
#define GRAY700 GetColor(ImGui::Spectrum::ColorEnum::GRAY700)
#define GRAY800 GetColor(ImGui::Spectrum::ColorEnum::GRAY800)
#define GRAY900 GetColor(ImGui::Spectrum::ColorEnum::GRAY900)
#define BLUE400 GetColor(ImGui::Spectrum::ColorEnum::BLUE400)
#define BLUE500 GetColor(ImGui::Spectrum::ColorEnum::BLUE500)
#define BLUE600 GetColor(ImGui::Spectrum::ColorEnum::BLUE600)
#define BLUE700 GetColor(ImGui::Spectrum::ColorEnum::BLUE700)
#define RED400 GetColor(ImGui::Spectrum::ColorEnum::RED400)
#define RED500 GetColor(ImGui::Spectrum::ColorEnum::RED500)
#define RED600 GetColor(ImGui::Spectrum::ColorEnum::RED600)
#define RED700 GetColor(ImGui::Spectrum::ColorEnum::RED700)
#define ORANGE400 GetColor(ImGui::Spectrum::ColorEnum::ORANGE400)
#define ORANGE500 GetColor(ImGui::Spectrum::ColorEnum::ORANGE500)
#define ORANGE600 GetColor(ImGui::Spectrum::ColorEnum::ORANGE600)
#define ORANGE700 GetColor(ImGui::Spectrum::ColorEnum::ORANGE700)
#define GREEN400 GetColor(ImGui::Spectrum::ColorEnum::GREEN400)
#define GREEN500 GetColor(ImGui::Spectrum::ColorEnum::GREEN500)
#define GREEN600 GetColor(ImGui::Spectrum::ColorEnum::GREEN600)
#define GREEN700 GetColor(ImGui::Spectrum::ColorEnum::GREEN700)
#define INDIGO400 GetColor(ImGui::Spectrum::ColorEnum::INDIGO400)
#define INDIGO500 GetColor(ImGui::Spectrum::ColorEnum::INDIGO500)
#define INDIGO600 GetColor(ImGui::Spectrum::ColorEnum::INDIGO600)
#define INDIGO700 GetColor(ImGui::Spectrum::ColorEnum::INDIGO700)
#define CELERY400 GetColor(ImGui::Spectrum::ColorEnum::CELERY400)
#define CELERY500 GetColor(ImGui::Spectrum::ColorEnum::CELERY500)
#define CELERY600 GetColor(ImGui::Spectrum::ColorEnum::CELERY600)
#define CELERY700 GetColor(ImGui::Spectrum::ColorEnum::CELERY700)
#define MAGENTA400 GetColor(ImGui::Spectrum::ColorEnum::MAGENTA400)
#define MAGENTA500 GetColor(ImGui::Spectrum::ColorEnum::MAGENTA500)
#define MAGENTA600 GetColor(ImGui::Spectrum::ColorEnum::MAGENTA600)
#define MAGENTA700 GetColor(ImGui::Spectrum::ColorEnum::MAGENTA700)
#define YELLOW400 GetColor(ImGui::Spectrum::ColorEnum::YELLOW400)
#define YELLOW500 GetColor(ImGui::Spectrum::ColorEnum::YELLOW500)
#define YELLOW600 GetColor(ImGui::Spectrum::ColorEnum::YELLOW600)
#define YELLOW700 GetColor(ImGui::Spectrum::ColorEnum::YELLOW700)
#define FUCHSIA400 GetColor(ImGui::Spectrum::ColorEnum::FUCHSIA400)
#define FUCHSIA500 GetColor(ImGui::Spectrum::ColorEnum::FUCHSIA500)
#define FUCHSIA600 GetColor(ImGui::Spectrum::ColorEnum::FUCHSIA600)
#define FUCHSIA700 GetColor(ImGui::Spectrum::ColorEnum::FUCHSIA700)
#define SEAFOAM400 GetColor(ImGui::Spectrum::ColorEnum::SEAFOAM400)
#define SEAFOAM500 GetColor(ImGui::Spectrum::ColorEnum::SEAFOAM500)
#define SEAFOAM600 GetColor(ImGui::Spectrum::ColorEnum::SEAFOAM600)
#define SEAFOAM700 GetColor(ImGui::Spectrum::ColorEnum::SEAFOAM700)
#define CHARTREUSE400 GetColor(ImGui::Spectrum::ColorEnum::CHARTREUSE400)
#define CHARTREUSE500 GetColor(ImGui::Spectrum::ColorEnum::CHARTREUSE500)
#define CHARTREUSE600 GetColor(ImGui::Spectrum::ColorEnum::CHARTREUSE600)
#define CHARTREUSE700 GetColor(ImGui::Spectrum::ColorEnum::CHARTREUSE700)
#define PURPLE400 GetColor(ImGui::Spectrum::ColorEnum::PURPLE400)
#define PURPLE500 GetColor(ImGui::Spectrum::ColorEnum::PURPLE500)
#define PURPLE600 GetColor(ImGui::Spectrum::ColorEnum::PURPLE600)
#define PURPLE700 GetColor(ImGui::Spectrum::ColorEnum::PURPLE700)
