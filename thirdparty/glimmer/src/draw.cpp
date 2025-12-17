#include "draw.h"
#include "context.h"
#include "style.h"
#include "imrichtext.h"

#include <unordered_map>

auto HomeIconSVG = R"(
<svg class="w-6 h-6 text-gray-800 dark:text-white" aria-hidden="true" width="24" height="24" fill="currentColor" viewBox="0 0 24 24">
  <path fill-rule="evenodd" d="M11.293 3.293a1 1 0 0 1 1.414 0l6 6 2 2a1 1 0 0 1-1.414 1.414L19 12.414V19a2 2 0 0 1-2 2h-3a1 1 0 0 1-1-1v-3h-2v3a1 1 0 0 1-1 1H7a2 2 0 0 1-2-2v-6.586l-.293.293a1 1 0 0 1-1.414-1.414l2-2 6-6Z" clip-rule="evenodd"/>
</svg>
)";

auto BrowseIconSVG = R"(
<svg width="800px" height="800px" viewBox="0 0 32 32" id="OBJECT">
<defs><style>.cls-1{fill:#b2b2b2;}</style></defs>
<path class="cls-1" d="M24,8H12.54L10.83,5.45A1,1,0,0,0,10,5H8A3,3,0,0,0,5,8v5a1,1,0,0,0,1,1H26a1,1,0,0,0,1-1V11A3,3,0,0,0,24,8Z"/>
<rect height="6" rx="1" ry="1" width="26" x="3" y="12"/>
<path class="cls-1" d="M30.81,16.42A1,1,0,0,0,30,16H2a1,1,0,0,0-.81.42,1,1,0,0,0-.14.9l3,9A1,1,0,0,0,5,27H27a1,1,0,0,0,.95-.68l3-9A1,1,0,0,0,30.81,16.42Z"/>
<path d="M10,24H8a1,1,0,0,1,0-2h2a1,1,0,0,1,0,2Z"/>
</svg>
)";

auto FileCopyIconSVG = R"(
<svg width="800px" height="800px" viewBox="0 0 24 24" fill="none">
<path d="M6.59961 11.3974C6.59961 8.67119 6.59961 7.3081 7.44314 6.46118C8.28667 5.61426 9.64432 5.61426 12.3596 5.61426H15.2396C17.9549 5.61426 19.3125 5.61426 20.1561 6.46118C20.9996 7.3081 20.9996 8.6712 20.9996 11.3974V16.2167C20.9996 18.9429 20.9996 20.306 20.1561 21.1529C19.3125 21.9998 17.9549 21.9998 15.2396 21.9998H12.3596C9.64432 21.9998 8.28667 21.9998 7.44314 21.1529C6.59961 20.306 6.59961 18.9429 6.59961 16.2167V11.3974Z" fill="#1C274C"/>
<path opacity="0.5" d="M4.17157 3.17157C3 4.34315 3 6.22876 3 10V12C3 15.7712 3 17.6569 4.17157 18.8284C4.78913 19.446 5.6051 19.738 6.79105 19.8761C6.59961 19.0353 6.59961 17.8796 6.59961 16.2167V11.3974C6.59961 8.6712 6.59961 7.3081 7.44314 6.46118C8.28667 5.61426 9.64432 5.61426 12.3596 5.61426H15.2396C16.8915 5.61426 18.0409 5.61426 18.8777 5.80494C18.7403 4.61146 18.4484 3.79154 17.8284 3.17157C16.6569 2 14.7712 2 11 2C7.22876 2 5.34315 2 4.17157 3.17157Z" fill="#1C274C"/>
</svg>
)";

auto SearchIconSVG = R"(
<svg fill="#000000" width="800px" height="800px" viewBox="0 0 24 24" id="search-alt" data-name="Flat Line" class="icon flat-line">
<circle id="secondary" cx="10.5" cy="10.5" r="7.5" style="fill: #b2b2b2; stroke-width: 2;"></circle>
<path id="primary" d="M21,21l-5.2-5.2M10.5,3A7.5,7.5,0,1,0,18,10.5,7.5,7.5,0,0,0,10.5,3Z" style="fill: none; stroke: #1C274C; stroke-linecap: round; stroke-linejoin: round; stroke-width: 2;"></path>
</svg>
)";

auto PinIconSVG = R"(
<svg width="800px" height="800px" viewBox="0 0 64 64" fill="none">
<path d="M33.9434 39.42V55.806C33.9434 56.3365 33.7327 56.8452 33.3576 57.2203C32.9826 57.5953 32.4739 57.806 31.9434 57.806C31.413 57.806 30.9043 57.5953 30.5292 57.2203C30.1541 56.8452 29.9434 56.3365 29.9434 55.806V39.42H33.9434Z" fill="#000000"/>
<path d="M44.7334 32.62L41.4334 29.26L44.5534 10.66C44.6493 10.0873 44.6192 9.50056 44.4654 8.9406C44.3116 8.38064 44.0377 7.86088 43.6628 7.41744C43.2879 6.974 42.8209 6.6175 42.2943 6.37273C41.7677 6.12797 41.1941 6.00079 40.6134 6.00003H23.2734C22.6914 5.9976 22.1158 6.12256 21.5871 6.36614C21.0584 6.60972 20.5895 6.96603 20.2131 7.41006C19.8367 7.85409 19.562 8.37509 19.4084 8.93652C19.2547 9.49796 19.2257 10.0862 19.3234 10.66L22.4434 29.26L19.1434 32.62C18.5918 33.1828 18.2187 33.8959 18.071 34.67C17.9232 35.444 18.0073 36.2444 18.3129 36.9708C18.6184 37.6972 19.1316 38.3172 19.7882 38.7529C20.4448 39.1886 21.2154 39.4207 22.0034 39.42H41.8834C42.6716 39.4211 43.4425 39.189 44.0991 38.7529C44.7557 38.3168 45.2686 37.6962 45.5733 36.9693C45.8779 36.2423 45.9608 35.4415 45.8113 34.6676C45.6619 33.8937 45.2869 33.1813 44.7334 32.62Z" fill="#999999"/>
</svg>
)";

auto SpannerIconSVG = R"(
<svg width="800px" height="800px" viewBox="0 0 14 14" role="img" focusable="false" aria-hidden="true">
<path fill="#607d8b" d="M12.874998 3.3650124l-2.480791 2.452277s-.7413861 0-1.4827721-.741386-.741386-1.482772-.741386-1.482772l2.4807921-2.480791c-1.3972281-.34217801-3.0796036.114059-4.1061379 1.140594-1.5398016 1.539801-.1710891 3.507325-.5702969 3.906533-2.1671281 2.252673-4.3912858 3.9635626-4.4483155 4.0491076-.6558414.655841-.6843562 1.71089-.05703 2.338217.6273266.627326 1.6823758.598812 2.3382171-.05703.085545-.08554 1.9104945-2.395247 4.0491078-4.5338596.3992083-.399208 2.2811874 1.055049 3.8780194-.513267.998019-1.026534 1.482771-2.680395 1.140593-4.077623zM2.8092586 11.94798c-.3992078 0-.712871-.313663-.712871-.712871 0-.427723.3136632-.741386.712871-.741386.3992078 0 .7128711.313663.7128711.712871s-.3136633.741386-.7128711.741386z"/>
</svg>
)";

auto GearsIconSVG = R"(
<svg version="1.1" id="gear" x="0px"
	 y="0px" width="800px" height="800px" viewBox="0 0 256 256" xml:space="preserve">
<style type="text/css">
	.st0{fill:#464646;}
	.st1{fill:#D1D1D1;}
</style>
<path class="st1" d="M120.502,243.45c-7.057,0-12.089-3.771-14.169-10.614c-1.694-5.572-3.675-11.136-5.488-16.045
	c-5.033-2.337-9.632-4.373-13.972-6.186c-4.677,2.092-9.771,4.521-15.837,7.556c-2.135,1.067-4.202,1.588-6.317,1.588
	c-3.589,0-6.82-1.467-9.604-4.36c-4.43-4.604-9.104-9.281-14.287-14.297c-4.448-4.31-5.426-9.978-2.688-15.554
	c1.102-2.242,2.145-4.495,3.249-6.881c1.337-2.888,2.713-5.86,4.208-8.822c-2.522-5.034-4.6-9.745-6.319-14.332
	c-2.427-0.81-4.823-1.636-7.154-2.44c-3.288-1.136-6.688-2.31-10.001-3.367c-6.163-1.965-9.636-6.771-9.523-13.18
	c0.124-7.103,0.123-13.59-0.005-19.833c-0.134-6.536,3.387-11.449,9.659-13.479c6.262-2.027,11.917-3.97,17.224-5.915
	c2.284-4.716,4.417-9.365,6.36-13.856c-2.061-4.599-4.436-9.577-7.541-15.818c-2.843-5.714-1.837-11.523,2.758-15.938
	C46.52,46.429,50.95,41.993,55,37.711c3.443-3.639,7.163-4.403,9.677-4.403c2.871,0,5.152,0.971,6.618,1.701
	c4.749,2.363,10.278,5.081,15.908,7.647c4.799-1.814,9.421-3.674,13.775-5.543c2.271-5.772,4.281-11.491,5.816-15.945
	c2.024-5.87,6.725-9.237,12.897-9.237l0.31,0.003c3.223,0.055,6.583,0.083,9.99,0.083c3.291,0,6.745-0.026,10.269-0.077
	c0.049,0,0.194-0.001,0.243-0.001c6.031,0,10.636,3.337,12.63,9.156c1.055,3.074,2.201,6.225,3.311,9.271
	c0.795,2.187,1.613,4.432,2.412,6.691c2.826,0.999,5.541,2.074,8.184,3.121c2.01,0.797,3.929,1.558,5.841,2.265
	c4.817-2.154,9.992-4.623,15.752-7.515c2.28-1.143,4.438-1.698,6.609-1.698c2.517,0,6.241,0.768,9.702,4.428
	c2.16,2.287,4.453,4.602,6.668,6.84c2.535,2.56,5.156,5.208,7.707,7.933c3.396,3.634,4.416,9.831,2.375,14.418
	c-1.459,3.273-2.996,6.502-4.481,9.625c-1.196,2.517-2.344,4.923-3.444,7.339c0.735,1.95,1.518,3.902,2.334,5.944
	c1.01,2.531,2.051,5.135,3.029,7.801c2.512,0.871,4.971,1.789,7.365,2.682c2.755,1.028,5.355,1.999,7.988,2.889
	c6.246,2.11,9.748,7.138,9.604,13.795c-0.169,7.626-0.163,13.473,0.019,18.958c0.129,3.931-0.979,10.978-9.853,14.025
	c-5.563,1.92-10.539,3.747-15.132,5.561c-1.771,4.445-3.51,9.075-5.18,13.81c1.066,2.322,2.188,4.671,3.274,6.955
	c1.501,3.149,3.054,6.406,4.526,9.703c1.855,4.139,1.73,10.467-2.109,14.534c-1.955,2.072-4.064,4.261-6.449,6.688
	c-0.813,0.83-1.635,1.658-2.453,2.486c-2.225,2.248-4.323,4.373-6.383,6.566c-1.941,2.07-4.404,3.368-7.123,3.753
	c-0.545,0.078-1.113,0.119-1.68,0.119c-3.387,0-5.902-1.405-7.254-2.16c-0.12-0.065-0.238-0.136-0.355-0.194
	c-0.918-0.447-1.864-0.879-2.813-1.312c-0.58-0.265-1.161-0.526-1.738-0.798c-2.637-1.227-5.269-2.457-7.888-3.721
	c-0.771-0.371-1.542-0.746-2.313-1.125c-4.754,2.231-9.517,4.287-14.189,6.129c-0.671,1.979-1.355,3.938-2.025,5.85
	c-1.308,3.725-2.539,7.24-3.646,10.813c-1.938,6.263-6.806,9.853-13.354,9.854l-0.438-0.005c-2.704-0.056-6.269-0.115-9.788-0.115
	c-3.08,0-5.842,0.045-8.444,0.14C121.024,243.445,120.759,243.45,120.502,243.45z"/>
<path class="st0" d="M195.82,210.063c-1.146,0.164-2.348-0.763-3.484-1.314c-1.563-0.764-3.158-1.462-4.734-2.195
	c-2.595-1.205-5.186-2.418-7.763-3.658c-1.417-0.685-2.831-1.375-4.237-2.08c-1.968-0.986-3.306-0.686-5.166,0.203
	c-5.42,2.586-10.939,4.983-16.541,7.145c-1.967,0.76-3.1,1.723-3.75,3.688c-2.078,6.271-4.424,12.457-6.375,18.767
	c-0.709,2.295-1.832,2.849-4.035,2.804c-6.262-0.124-12.534-0.195-18.792,0.026c-2.833,0.102-4.19-0.716-5.041-3.515
	c-1.92-6.315-4.158-12.543-6.466-18.729c-0.411-1.104-1.624-2.188-2.735-2.709c-5.756-2.697-11.562-5.3-17.441-7.72
	c-1.314-0.539-3.216-0.726-4.457-0.184c-6.159,2.697-12.223,5.622-18.239,8.631c-1.796,0.897-2.866,0.67-4.243-0.763
	c-4.754-4.94-9.613-9.783-14.539-14.55c-1.334-1.293-1.469-2.322-0.666-3.959c2.758-5.613,5.207-11.383,8.085-16.932
	c1.313-2.533,0.983-4.408-0.252-6.845c-2.554-5.034-4.889-10.216-6.802-15.521c-0.84-2.329-2.021-3.426-4.199-4.137
	c-6.281-2.056-12.488-4.334-18.782-6.346c-1.899-0.604-2.604-1.463-2.568-3.479c0.118-6.736,0.132-13.479-0.006-20.213
	c-0.044-2.168,0.733-3.111,2.741-3.761c6.51-2.108,12.989-4.324,19.401-6.71c1.215-0.451,2.441-1.693,3.024-2.883
	c2.793-5.71,5.458-11.489,7.95-17.336c0.544-1.278,0.704-3.164,0.168-4.385c-2.704-6.158-5.63-12.22-8.627-18.243
	c-0.883-1.774-0.723-2.874,0.733-4.272c4.86-4.666,9.653-9.411,14.282-14.305c1.564-1.654,2.781-1.514,4.571-0.623
	c6.022,2.997,12.071,5.948,18.21,8.695c1.161,0.521,2.918,0.394,4.163-0.07c5.737-2.141,11.432-4.416,17.047-6.859
	c1.308-0.569,2.758-1.778,3.269-3.045c2.421-6.014,4.61-12.124,6.724-18.254c0.65-1.885,1.604-2.528,3.586-2.495
	c6.855,0.117,13.716,0.105,20.57,0.006c1.871-0.026,2.682,0.685,3.27,2.399c2.064,6.024,4.348,11.975,6.434,17.993
	c0.677,1.947,1.73,2.908,3.799,3.598c5.693,1.901,11.197,4.361,16.853,6.383c1.339,0.478,3.195,0.587,4.447,0.037
	c6.048-2.657,12.009-5.52,17.913-8.484c1.857-0.932,3.043-0.94,4.559,0.663c4.709,4.98,9.657,9.733,14.342,14.737
	c0.688,0.735,0.957,2.579,0.539,3.518c-2.832,6.357-5.986,12.571-8.813,18.931c-0.513,1.147-0.353,2.879,0.104,4.12
	c2.023,5.527,4.396,10.929,6.366,16.474c0.729,2.046,1.563,3.333,3.759,4.061c5.819,1.93,11.504,4.272,17.313,6.236
	c2.182,0.737,2.855,1.861,2.807,4.104c-0.145,6.498-0.193,13.007,0.021,19.504c0.082,2.53-0.924,3.489-3.106,4.239
	c-5.909,2.039-11.786,4.188-17.583,6.523c-1.191,0.479-2.386,1.806-2.879,3.023c-2.385,5.891-4.609,11.85-6.693,17.852
	c-0.396,1.146-0.278,2.766,0.227,3.883c2.764,6.132,5.798,12.142,8.542,18.281c0.444,0.995,0.401,2.897-0.252,3.592
	c-2.08,2.205-4.188,4.382-6.313,6.543c-3.01,3.065-6.059,6.091-8.996,9.225C196.652,209.797,196.24,210.004,195.82,210.063z
	 M172.083,126.68c0.065-23.207-18.606-42.196-41.567-42.272c-23.586-0.079-42.52,18.377-42.604,41.532
	c-0.087,23.701,18.489,42.607,41.895,42.64C153.2,168.614,172.018,149.965,172.083,126.68z"/>
</svg>
)";

auto WarningIconSVG = R"(
<svg width="800px" height="800px" viewBox="0 0 32 32" fill="none">
  <path stroke="#535358" stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M15.12 4.623a1 1 0 011.76 0l11.32 20.9A1 1 0 0127.321 27H4.679a1 1 0 01-.88-1.476l11.322-20.9zM16 18v-6"/>
  <path fill="#535358" d="M17.5 22.5a1.5 1.5 0 11-3 0 1.5 1.5 0 013 0z"/>
</svg>
)";

auto ErrorIconSVG = R"(
<svg width="800px" height="800px" viewBox="0 0 24 24" fill="none"">
    <path d="M3.23 7.913 7.91 3.23c.15-.15.35-.23.57-.23h7.05c.21 0 .42.08.57.23l4.67 4.673c.15.15.23.35.23.57v7.054c0 .21-.08.42-.23.57L16.1 20.77c-.15.15-.35.23-.57.23H8.47a.81.81 0 0 1-.57-.23l-4.67-4.673a.793.793 0 0 1-.23-.57V8.473c0-.21.08-.42.23-.57v.01Z" fill="#000000" fill-opacity=".16" stroke="#000000" stroke-width="1.5" stroke-miterlimit="10" stroke-linejoin="round"/>
    <path d="M12 16h.008M12 8v5" stroke="#000000" stroke-width="1.5" stroke-miterlimit="10" stroke-linecap="round"/>
</svg>
)";

auto InfoIconSVG = R"(
<svg width="800px" height="800px" viewBox="0 0 24 24" fill="none">
<path opacity="0.15" d="M21 12C21 16.9706 16.9706 21 12 21C7.02944 21 3 16.9706 3 12C3 7.02944 7.02944 3 12 3C16.9706 3 21 7.02944 21 12Z" fill="#000000"/>
<path d="M12 7.01001V7.00002M12 17L12 10M21 12C21 16.9706 16.9706 21 12 21C7.02944 21 3 16.9706 3 12C3 7.02944 7.02944 3 12 3C16.9706 3 21 7.02944 21 12Z" stroke="#000000" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"/>
</svg>
)";

namespace glimmer
{
    IntersectRects ComputeIntersectRects(ImRect rect, ImVec2 startpos, ImVec2 endpos)
    {
        IntersectRects res;

        res.intersects[0].Max.x = startpos.x;
        res.intersects[1].Max.y = startpos.y;
        res.intersects[2].Min.x = endpos.x;
        res.intersects[3].Min.y = endpos.y;

        if (rect.Min.x < startpos.x)
        {
            res.intersects[0].Min.x = res.intersects[1].Min.x = res.intersects[3].Min.x = rect.Min.x;
        }
        else
        {
            res.intersects[1].Min.x = res.intersects[3].Min.x = rect.Min.x;
            res.visibleRect[0] = false;
        }

        if (rect.Min.y < startpos.y)
        {
            res.intersects[0].Min.y = res.intersects[1].Min.y = res.intersects[2].Min.y = rect.Min.y;
        }
        else
        {
            res.intersects[0].Min.y = res.intersects[2].Min.y = rect.Min.y;
            res.visibleRect[1] = false;
        }

        if (rect.Max.x > endpos.x)
        {
            res.intersects[1].Max.x = res.intersects[2].Max.x = res.intersects[3].Max.x = rect.Max.x;
        }
        else
        {
            res.intersects[1].Max.x = res.intersects[3].Max.x = rect.Max.x;
            res.visibleRect[2] = false;
        }

        if (rect.Max.y > endpos.y)
        {
            res.intersects[0].Max.y = res.intersects[2].Max.y = res.intersects[3].Max.y = rect.Max.y;
        }
        else
        {
            res.intersects[0].Max.y = res.intersects[2].Max.y = rect.Max.y;
            res.visibleRect[3] = false;
        }

        return res;
    }

    RectBreakup ComputeRectBreakups(ImRect rect, float amount)
    {
        RectBreakup res;

        res.rects[0].Min = ImVec2{ rect.Min.x - amount, rect.Min.y };
        res.rects[0].Max = ImVec2{ rect.Min.x, rect.Max.y };

        res.rects[1].Min = ImVec2{ rect.Min.x, rect.Min.y - amount };
        res.rects[1].Max = ImVec2{ rect.Max.x, rect.Min.y };

        res.rects[2].Min = ImVec2{ rect.Max.x, rect.Min.y };
        res.rects[2].Max = ImVec2{ rect.Max.x + amount, rect.Max.y };

        res.rects[3].Min = ImVec2{ rect.Min.x, rect.Max.y };
        res.rects[3].Max = ImVec2{ rect.Max.x, rect.Max.y + amount };

        ImVec2 delta{ amount, amount };
        res.corners[0].Min = rect.Min - delta;
        res.corners[0].Max = rect.Min;
        res.corners[1].Min = ImVec2{ rect.Max.x, rect.Min.y - amount };
        res.corners[1].Max = res.corners[1].Min + delta;
        res.corners[2].Min = rect.Max;
        res.corners[2].Max = rect.Max + delta;
        res.corners[3].Min = ImVec2{ rect.Min.x - amount, rect.Max.y };
        res.corners[3].Max = res.corners[3].Min + delta;

        return res;
    }

    void DrawBorderRect(ImVec2 startpos, ImVec2 endpos, const FourSidedBorder& border, uint32_t bgcolor, IRenderer& renderer)
    {
        if (border.isUniform && border.top.thickness > 0.f && IsColorVisible(border.top.color) &&
            border.top.color != bgcolor)
        {
            if (!border.isRounded())
                renderer.DrawRect(startpos, endpos, border.top.color, false, border.top.thickness);
            else
                renderer.DrawRoundedRect(startpos, endpos, border.top.color, false,
                    border.cornerRadius[TopLeftCorner], border.cornerRadius[TopRightCorner],
                    border.cornerRadius[BottomRightCorner], border.cornerRadius[BottomLeftCorner],
                    border.top.thickness);
        }
        else
        {
            auto width = endpos.x - startpos.x, height = endpos.y - startpos.y;

            if (border.top.thickness > 0.f && border.top.color != bgcolor && IsColorVisible(border.top.color))
                renderer.DrawLine(startpos, startpos + ImVec2{ width, 0.f }, border.top.color, border.top.thickness);
            if (border.right.thickness > 0.f && border.right.color != bgcolor && IsColorVisible(border.right.color))
                renderer.DrawLine(startpos + ImVec2{ width - border.right.thickness, 0.f }, endpos -
                    ImVec2{ border.right.thickness, 0.f }, border.right.color, border.right.thickness);
            if (border.left.thickness > 0.f && border.left.color != bgcolor && IsColorVisible(border.left.color))
                renderer.DrawLine(startpos, startpos + ImVec2{ 0.f, height }, border.left.color, border.left.thickness);
            if (border.bottom.thickness > 0.f && border.bottom.color != bgcolor && IsColorVisible(border.bottom.color))
                renderer.DrawLine(startpos + ImVec2{ 0.f, height - border.bottom.thickness }, endpos -
                    ImVec2{ 0.f, border.bottom.thickness }, border.bottom.color, border.bottom.thickness);
        }
    }

    void DrawBoxShadow(ImVec2 startpos, ImVec2 endpos, const StyleDescriptor& style, IRenderer& renderer)
    {
        // In order to draw box-shadow, the following steps are used:
        // 1. Draw the underlying rectangle with shadow color, spread and offset.
        // 2. Decompose the blur region into 8 rects i.e. 4 for corners, 4 for the sides
        // 3. For each rect, determine the vertex color i.e. shadow color or transparent,
        //    and draw a rect gradient accordingly.
        if ((style.shadow.blur > 0.f || style.shadow.spread > 0.f || style.shadow.offset.x != 0.f ||
            style.shadow.offset.y != 0) && IsColorVisible(style.shadow.color))
        {
            ImRect rect{ startpos, endpos };
            rect.Expand(style.shadow.spread);
            rect.Translate(style.shadow.offset);

            if (style.shadow.blur > 0.f)
            {
                auto outercol = style.shadow.color & ~IM_COL32_A_MASK;
                auto brk = ComputeRectBreakups(rect, style.shadow.blur);

                // Sides: Left -> Top -> Right -> Bottom
                renderer.DrawRectGradient(brk.rects[0].Min, brk.rects[0].Max, outercol, style.shadow.color, DIR_Horizontal);
                renderer.DrawRectGradient(brk.rects[1].Min, brk.rects[1].Max, outercol, style.shadow.color, DIR_Vertical);
                renderer.DrawRectGradient(brk.rects[2].Min, brk.rects[2].Max, style.shadow.color, outercol, DIR_Horizontal);
                renderer.DrawRectGradient(brk.rects[3].Min, brk.rects[3].Max, style.shadow.color, outercol, DIR_Vertical);

                // Corners: Top-left -> Top-right -> Bottom-right -> Bottom-left
                switch (Config.shadowQuality)
                {
                case BoxShadowQuality::Balanced:
                {
                    ImVec2 center = brk.corners[0].Max;
                    auto radius = brk.corners[0].GetHeight() - 0.5f; // all corners of same size
                    ImVec2 offset{ radius / 2.f, radius / 2.f };
                    radius *= 1.75f;

                    renderer.SetClipRect(brk.corners[0].Min, brk.corners[0].Max);
                    renderer.DrawRadialGradient(center + offset, radius, style.shadow.color, outercol, 180, 270);
                    renderer.ResetClipRect();

                    center = ImVec2{ brk.corners[1].Min.x, brk.corners[1].Max.y };
                    renderer.SetClipRect(brk.corners[1].Min, brk.corners[1].Max);
                    renderer.DrawRadialGradient(center + ImVec2{ -offset.x, offset.y }, radius, style.shadow.color, outercol, 270, 360);
                    renderer.ResetClipRect();

                    center = brk.corners[2].Min;
                    renderer.SetClipRect(brk.corners[2].Min, brk.corners[2].Max);
                    renderer.DrawRadialGradient(center - offset, radius, style.shadow.color, outercol, 0, 90);
                    renderer.ResetClipRect();

                    center = ImVec2{ brk.corners[3].Max.x, brk.corners[3].Min.y };
                    renderer.SetClipRect(brk.corners[3].Min, brk.corners[3].Max);
                    renderer.DrawRadialGradient(center + ImVec2{ offset.x, -offset.y }, radius, style.shadow.color, outercol, 90, 180);
                    renderer.ResetClipRect();
                    break;
                }
                default:
                    break;
                }
            }

            rect.Expand(style.shadow.blur > 0.f ? 1.f : 0.f);
            if (!style.border.isRounded())
                renderer.DrawRect(rect.Min, rect.Max, style.shadow.color, true);
            else
                renderer.DrawRoundedRect(rect.Min, rect.Max, style.shadow.color, true,
                    style.border.cornerRadius[TopLeftCorner], style.border.cornerRadius[TopRightCorner],
                    style.border.cornerRadius[BottomRightCorner], style.border.cornerRadius[BottomLeftCorner]);

            auto diffcolor = Config.bgcolor - 1;
            auto border = style.border;
            border.setColor(Config.bgcolor);
            DrawBorderRect(startpos, endpos, style.border, diffcolor, renderer);
            if (!border.isRounded())
                renderer.DrawRect(startpos, endpos, Config.bgcolor, true);
            else
                renderer.DrawRoundedRect(startpos, endpos, Config.bgcolor, true,
                    style.border.cornerRadius[TopLeftCorner], style.border.cornerRadius[TopRightCorner],
                    style.border.cornerRadius[BottomRightCorner], style.border.cornerRadius[BottomLeftCorner]);
        }
    }

    void DrawBackground(ImVec2 startpos, ImVec2 endpos, const StyleDescriptor& style, IRenderer& renderer)
    {
        DrawBackground(startpos, endpos, style.bgcolor, style.gradient, style.border, renderer);
    }

    void DrawBackground(ImVec2 startpos, ImVec2 endpos, uint32_t bgcolor, const ColorGradient& gradient, const FourSidedBorder& border, IRenderer& renderer)
    {
        if (gradient.totalStops != 0)
            (gradient.dir == ImGuiDir_Down || gradient.dir == ImGuiDir_Left) ?
            DrawLinearGradient(startpos, endpos, gradient.angleDegrees, gradient.dir,
                std::begin(gradient.colorStops), std::begin(gradient.colorStops) + gradient.totalStops, border.isRounded(), renderer) :
            DrawLinearGradient(startpos, endpos, gradient.angleDegrees, gradient.dir,
                std::rbegin(gradient.colorStops), std::rbegin(gradient.colorStops) + gradient.totalStops, border.isRounded(), renderer);
        else if (IsColorVisible(bgcolor))
            if (!border.isRounded())
                renderer.DrawRect(startpos, endpos, bgcolor, true);
            else
                renderer.DrawRoundedRect(startpos, endpos, bgcolor, true,
                    border.cornerRadius[TopLeftCorner], border.cornerRadius[TopRightCorner],
                    border.cornerRadius[BottomRightCorner], border.cornerRadius[BottomLeftCorner]);
    }

    void DrawText(ImVec2 startpos, ImVec2 endpos, const ImRect& textrect, std::string_view text, bool disabled, 
        const StyleDescriptor& style, IRenderer& renderer, std::optional<int32_t> txtflags)
    {
        ImRect content{ startpos, endpos };
        renderer.SetClipRect(content.Min, content.Max);
        auto flags = txtflags.has_value() ? txtflags.value() : style.font.flags;

        if (flags & TextIsSVG)
        {
            auto sz = content.GetSize();
            sz.x = std::min(sz.x, sz.y);
            sz.y = sz.x;
            renderer.DrawResource(RT_SVG | RT_BIN, content.Min, sz, style.bgcolor, text);
        }
        else if (flags & TextIsPlainText)
        {
            renderer.SetCurrentFont(style.font.font, style.font.size);
            if ((flags & FontStyleOverflowMarquee) != 0 &&
                !disabled && style.index.animation != InvalidIdx)
            {
                auto& context = GetContext();
                auto& animation = context.animations[style.index.animation];
                ImVec2 textsz = textrect.GetWidth() == 0.f ? renderer.GetTextSize(text, style.font.font, style.font.size) :
                    textrect.GetSize();

                if (textsz.x > content.GetWidth())
                {
                    animation.moveByPixel(1.f, content.GetWidth(), -textsz.x);
                    content.Min.x += animation.offset;
                    renderer.DrawText(text, content.Min, style.fgcolor, style.dimension.x > 0 ? style.dimension.x : -1.f);
                }
                else
                    renderer.DrawText(text, textrect.Min, style.fgcolor, textrect.GetWidth());
            }
            else if (flags & FontStyleOverflowEllipsis)
            {
                ImVec2 textsz = textrect.GetWidth() == 0.f ? renderer.GetTextSize(text, style.font.font, style.font.size) :
                    textrect.GetSize();

                if (textsz.x > content.GetWidth())
                {
                    float width = 0.f, available = content.GetWidth() - renderer.EllipsisWidth(style.font.font, style.font.size);

                    for (auto chidx = 0; chidx < (int)text.size(); ++chidx)
                    {
                        // TODO: This is only valid for ASCII, this should be fixed for UTF-8
                        auto w = renderer.GetTextSize(text.substr(chidx, 1), style.font.font, style.font.size).x;
                        if (width + w > available)
                        {
                            renderer.DrawText(text.substr(0, chidx), content.Min, style.fgcolor, -1.f);
                            renderer.DrawText("...", content.Min + ImVec2{ width, 0.f }, style.fgcolor, -1.f);
                            break;
                        }
                        width += w;
                    }
                }
                else renderer.DrawText(text, textrect.Min, style.fgcolor, textrect.GetWidth());
            }
            else
                renderer.DrawText(text, textrect.Min, style.fgcolor, textrect.GetWidth());

            renderer.ResetFont();
        }
#ifndef GLIMMER_DISABLE_RICHTEXT
        else if (flags & TextIsRichText)
        {
            auto id = ImRichText::CreateRichText(text.data(), text.data() + text.size());
            ImRichText::Show(textrect.Min, id, textrect.GetSize());
        }
#endif
        
        renderer.ResetClipRect();
    }

    void DrawSymbol(ImVec2 startpos, ImVec2 size, ImVec2 padding, SymbolIcon symbol, uint32_t outlineColor, 
        uint32_t fillColor, float thickness, IRenderer& renderer)
    {
        startpos += padding;
        size -= (padding + padding);

        switch (symbol)
        {
        case glimmer::SymbolIcon::DownArrow:
        {
            ImVec2 start{ startpos.x, startpos.y }, end{ startpos.x + (size.x * 0.5f), startpos.y + size.y };
            renderer.DrawLine(start, end, outlineColor, thickness);
            renderer.DrawLine(end, ImVec2{ startpos.x + size.x, startpos.y }, outlineColor, thickness);
            break;
        }
        case glimmer::SymbolIcon::UpArrow:
        {
            ImVec2 start{ startpos.x, startpos.y + size.y }, end{ startpos.x + (size.x * 0.5f), startpos.y };
            renderer.DrawLine(start, end, outlineColor, thickness);
            renderer.DrawLine(end, ImVec2{ startpos.x + size.x, startpos.y + size.y }, outlineColor, thickness);
            break;
        }
        case glimmer::SymbolIcon::DownTriangle:
        {
            ImVec2 pos1{ startpos.x, startpos.y };
            ImVec2 pos2{ startpos.x + size.x, startpos.y };
            ImVec2 pos3{ startpos.x + (0.5f * size.x), startpos.y + size.y };
            renderer.DrawTriangle(pos1, pos2, pos3, fillColor, true);
            if (thickness > 0.f) renderer.DrawTriangle(pos1, pos2, pos3, outlineColor, false, thickness);
            break;
        }
        case glimmer::SymbolIcon::UpTriangle:
        {
            ImVec2 pos1{ startpos.x, startpos.y + size.y };
            ImVec2 pos2{ startpos.x + (0.5f * size.x), startpos.y };
            ImVec2 pos3{ startpos.x + size.x, startpos.y + size.y };
            renderer.DrawTriangle(pos1, pos2, pos3, fillColor, true);
            if (thickness > 0.f) renderer.DrawTriangle(pos1, pos2, pos3, outlineColor, false, thickness);
            break;
        }
        case glimmer::SymbolIcon::LeftTriangle:
        {
            ImVec2 pos1{ startpos.x, startpos.y + (0.5f * size.y) };
            ImVec2 pos2{ startpos.x + size.x, startpos.y };
            ImVec2 pos3{ startpos.x + size.x, startpos.y + size.y };
            renderer.DrawTriangle(pos1, pos2, pos3, fillColor, true);
            if (thickness > 0.f) renderer.DrawTriangle(pos1, pos2, pos3, outlineColor, false, thickness);
            break;
        }
        case glimmer::SymbolIcon::RightTriangle:
        {
            ImVec2 pos1{ startpos.x, startpos.y };
            ImVec2 pos2{ startpos.x + size.x, startpos.y + (0.5f * size.y) };
            ImVec2 pos3{ startpos.x, startpos.y + size.y };
            renderer.DrawTriangle(pos1, pos2, pos3, fillColor, true);
            if (thickness > 0.f) renderer.DrawTriangle(pos1, pos2, pos3, outlineColor, false, thickness);
            break;
        }
        case glimmer::SymbolIcon::Plus:
        {
            auto halfw = size.x * 0.5f, halfh = size.y * 0.5f;
            renderer.DrawLine(ImVec2{ startpos.x + halfw, startpos.y },
                ImVec2{ startpos.x + halfw , startpos.y + size.y }, outlineColor, thickness);
            renderer.DrawLine(ImVec2{ startpos.x, startpos.y + halfh },
                ImVec2{ startpos.x + size.x, startpos.y + halfh }, outlineColor, thickness);
            break;
        }
        case glimmer::SymbolIcon::Minus:
        {
            auto halfh = size.y * 0.5f;
            renderer.DrawLine(ImVec2{ startpos.x, startpos.y + halfh },
                ImVec2{ startpos.x + size.x, startpos.y + halfh }, outlineColor, thickness);
            break;
        }
        case glimmer::SymbolIcon::Cross:
        {
            renderer.DrawLine(ImVec2{ startpos.x, startpos.y },
                ImVec2{ startpos.x + size.x, startpos.y + size.y }, outlineColor, thickness);
            renderer.DrawLine(ImVec2{ startpos.x + size.x, startpos.y },
                ImVec2{ startpos.x, startpos.y + size.y }, outlineColor, thickness);
            break;
        }
        case SymbolIcon::Home:
        {
            renderer.DrawResource(RT_SVG | RT_BIN, startpos, size, outlineColor, HomeIconSVG);
            break;
        }
        case SymbolIcon::Browse:
        {
            renderer.DrawResource(RT_SVG | RT_BIN, startpos, size, outlineColor, BrowseIconSVG);
            break;
        }
        case SymbolIcon::Search:
        {
            renderer.DrawResource(RT_SVG | RT_BIN, startpos, size, outlineColor, SearchIconSVG);
            break;
        }
        case SymbolIcon::Gears:
        {
            renderer.DrawResource(RT_SVG | RT_BIN, startpos, size, outlineColor, GearsIconSVG);
            break;
        }
        case SymbolIcon::Spanner:
        {
            renderer.DrawResource(RT_SVG | RT_BIN, startpos, size, outlineColor, SpannerIconSVG);
            break;
        }
        case SymbolIcon::Copy:
        {
            renderer.DrawResource(RT_SVG | RT_BIN, startpos, size, outlineColor, FileCopyIconSVG);
            break;
        }
        case SymbolIcon::Pin:
        {
            renderer.DrawResource(RT_SVG | RT_BIN, startpos, size, outlineColor, PinIconSVG);
            break;
        }
        case SymbolIcon::Warning:
        {
            renderer.DrawResource(RT_SVG | RT_BIN, startpos, size, outlineColor, WarningIconSVG);
            break;
        }
        case SymbolIcon::Error:
        {
            renderer.DrawResource(RT_SVG | RT_BIN, startpos, size, outlineColor, ErrorIconSVG);
            break;
        }
        case SymbolIcon::Info:
        {
            renderer.DrawResource(RT_SVG | RT_BIN, startpos, size, outlineColor, InfoIconSVG);
            break;
        }
        default:
            break;
        }
    }

    SymbolIcon GetSymbolIcon(std::string_view name)
    {
        static const std::unordered_map<std::string_view, SymbolIcon> IconLookup{
            { "arrow-down", SymbolIcon::DownArrow },
            { "arrow-up", SymbolIcon::UpArrow },
            { "triangle-down", SymbolIcon::DownTriangle },
            { "triangle-up", SymbolIcon::UpTriangle },
            { "triangle-left", SymbolIcon::LeftTriangle },
            { "triangle-right", SymbolIcon::RightTriangle },
            { "plus", SymbolIcon::Plus },
            { "minus", SymbolIcon::Minus },
            { "cross", SymbolIcon::Cross },
            { "home", SymbolIcon::Home },
            { "browse", SymbolIcon::Browse },
            { "search", SymbolIcon::Search },
            { "gears", SymbolIcon::Gears },
            { "spanner", SymbolIcon::Spanner },
            { "copy", SymbolIcon::Copy },
            { "pin", SymbolIcon::Pin },
            { "warning", SymbolIcon::Warning },
            { "error", SymbolIcon::Error },
            { "info", SymbolIcon::Info }
        };

        auto it = IconLookup.find(name);
        return it == IconLookup.end() ? SymbolIcon::None : it->second;
    }
}