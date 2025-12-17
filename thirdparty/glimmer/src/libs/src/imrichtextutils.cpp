#ifndef GLIMMER_DISABLE_RICHTEXT
#include "imrichtextutils.h"

#include <cctype>
#include <unordered_map>

#include "renderer.h"

namespace ImRichText
{
    void ParseRichText(const char* text, const char* textend, char TagStart, char TagEnd, ITagVisitor& visitor)
    {
        int end = (int)(textend - text), start = 0;
        start = SkipSpace(text, start, end);
        auto isPreformattedContent = false;
        std::string_view lastTag = "";

        for (auto idx = start; idx < end;)
        {
            if (text[idx] == TagStart)
            {
                idx++;
                auto tagStart = true, selfTerminatingTag = false;
                auto [currTag, status] = glimmer::ExtractTag(text, end, TagEnd, idx, tagStart);
                if (!status) { visitor.Error(currTag); return; }

                isPreformattedContent = visitor.IsPreformattedContent(currTag);
                lastTag = currTag;

                if (tagStart)
                {
                    if (!visitor.TagStart(currTag)) return;

                    while ((idx < end) && (text[idx] != TagEnd) && (text[idx] != '/'))
                    {
                        auto begin = idx;
                        while ((idx < end) && (text[idx] != '=') && !std::isspace(text[idx]) && (text[idx] != '/')) idx++;

                        if (text[idx] != '/')
                        {
                            auto attribName = std::string_view{ text + begin, (std::size_t)(idx - begin) };
                            
                            idx = SkipSpace(text, idx, end);
                            if (text[idx] == '=') idx++;
                            idx = SkipSpace(text, idx, end);
                            auto attribValue = GetQuotedString(text, idx, end);
                            if (!visitor.Attribute(attribName, attribValue)) return;
                        }
                    }

                    if (text[idx] == TagEnd) idx++;
                    if (text[idx] == '/' && ((idx + 1) < end) && text[idx + 1] == TagEnd) idx += 2;
                }

                selfTerminatingTag = (text[idx - 2] == '/' && text[idx - 1] == TagEnd) || visitor.IsSelfTerminating(currTag);

                if (selfTerminatingTag || !tagStart) {
                    if (!visitor.TagEnd(currTag, selfTerminatingTag)) return;
                }
                else if (!selfTerminatingTag && tagStart)
                    if (!visitor.TagStartDone()) return;
            }
            else
            {
                auto begin = idx;

                if (isPreformattedContent)
                {
                    static char EndTag[64] = { 0 };
                    EndTag[0] = TagStart; EndTag[1] = '/';
                    std::memcpy(EndTag + 2, lastTag.data(), lastTag.size());
                    EndTag[2u + lastTag.size()] = TagEnd;
                    EndTag[3u + lastTag.size()] = 0;

                    while (((idx + (int)(lastTag.size() + 3u)) < end) &&
                        AreSame(std::string_view{ text + idx, lastTag.size() + 3u }, EndTag)) idx++;
                    std::string_view content{ text + begin, (std::size_t)(idx - begin) };

                    if (!visitor.Content(content)) return;
                }
                else
                {
                    while ((idx < end) && (text[idx] != TagStart)) idx++;
                    std::string_view content{ text + begin, (std::size_t)(idx - begin) };
                    if (!visitor.Content(content)) return;
                }
            }
        }

        visitor.Finalize();
    }

    float IRenderer::EllipsisWidth(void* fontptr, float sz)
    {
        auto renderer = (glimmer::IRenderer*)UserData;
        return renderer->GetTextSize("...", fontptr, sz).x;
    }

    void IRenderer::DrawBullet(ImVec2 startpos, ImVec2 endpos, uint32_t color, int index, int depth)
    {
    }

    void IRenderer::DrawDefaultBullet(BulletType type, ImVec2 initpos, const BoundedBox& bounds, uint32_t color, float bulletsz)
    {
        // TODO: If font loaded already contain these shapes, use them instead?
        auto renderer = (glimmer::IRenderer*)UserData;

        switch (type)
        {
        case BulletType::Circle: {
            ImVec2 center = bounds.center(initpos);
            renderer->DrawCircle(center, bulletsz * 0.5f, color, false);
            break;
        }

        case BulletType::Disk: {
            ImVec2 center = bounds.center(initpos);
            renderer->DrawCircle(center, bulletsz * 0.5f, color, true);
            break;
        }

        case BulletType::Square: {
            renderer->DrawRect(bounds.start(initpos), bounds.end(initpos), color, true);
            break;
        }

        case BulletType::Concentric: {
            ImVec2 center = bounds.center(initpos);
            renderer->DrawCircle(center, bulletsz * 0.5f, color, false);
            renderer->DrawCircle(center, bulletsz * 0.4f, color, true);
            break;
        }

        case BulletType::Triangle: {
            auto startpos = bounds.start(initpos);
            auto offset = bulletsz * 0.25f;
            ImVec2 a{ startpos.x, startpos.y },
                b{ startpos.x + bulletsz, startpos.y + (bulletsz * 0.5f) },
                c{ startpos.x, startpos.y + bulletsz };
            renderer->DrawTriangle(a, b, c, color, true);
            break;
        }

        case BulletType::Arrow: {
            auto startpos = bounds.start(initpos);
            auto bsz2 = bulletsz * 0.5f;
            auto bsz3 = bulletsz * 0.33333f;
            auto bsz6 = bsz3 * 0.5f;
            auto bsz38 = bulletsz * 0.375f;
            ImVec2 points[7];
            points[0] = { startpos.x, startpos.y + bsz38 };
            points[1] = { startpos.x + bsz2, startpos.y + bsz38 };
            points[2] = { startpos.x + bsz2, startpos.y + bsz6 };
            points[3] = { startpos.x + bulletsz, startpos.y + bsz2 };
            points[4] = { startpos.x + bsz2, startpos.y + bulletsz - bsz6 };
            points[5] = { startpos.x + bsz2, startpos.y + bulletsz - bsz38 };
            points[6] = { startpos.x, startpos.y + bulletsz - bsz38 };
            renderer->DrawRect(points[0], points[5], color, true);
            renderer->DrawTriangle(points[2], points[3], points[4], color, true);
            break;
        }

        case BulletType::CheckMark: {
            auto startpos = bounds.start(initpos);
            auto bsz3 = (bulletsz * 0.25f);
            auto thickness = bulletsz * 0.2f;
            ImVec2 points[3];
            points[0] = { startpos.x, startpos.y + (2.5f * bsz3) };
            points[1] = { startpos.x + (bulletsz * 0.3333f), startpos.y + bulletsz };
            points[2] = { startpos.x + bulletsz, startpos.y + bsz3 };
            renderer->DrawPolyline(points, 3, color, thickness);
            break;
        }

        // TODO: Fix this
        case BulletType::CheckBox: {
            auto startpos = bounds.start(initpos);
            auto checkpos = ImVec2{ startpos.x + (bulletsz * 0.25f), startpos.y + (bulletsz * 0.25f) };
            bulletsz *= 0.75f;
            auto bsz3 = (bulletsz * 0.25f);
            auto thickness = bulletsz * 0.25f;
            ImVec2 points[3];
            points[0] = { checkpos.x, checkpos.y + (2.5f * bsz3) };
            points[1] = { checkpos.x + (bulletsz * 0.3333f), checkpos.y + bulletsz };
            points[2] = { checkpos.x + bulletsz, checkpos.y + bsz3 };
            renderer->DrawPolyline(points, 3, color, thickness);
            renderer->DrawRect(startpos, bounds.end(initpos), color, false, thickness);
            break;
        }

        default:
            break;
        }
    }
}
#endif