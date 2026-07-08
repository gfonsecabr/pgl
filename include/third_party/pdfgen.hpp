#pragma once

/**
 * @file pdfgen.hpp
 * @brief Trimmed header-only C++ port of PDFGen for Pangolin canvas export.
 *
 * Source: https://github.com/AndreRenaud/PDFGen
 * Original license: The Unlicense / public domain. The upstream project states:
 * "This is free and unencumbered software released into the public domain."
 * This file is a trimmed and modified port for use inside pgl. Image loading,
 * barcodes, TrueType embedding, encryption, bookmarks, and link annotations
 * were removed; the retained subset covers document creation/serialization,
 * pages, standard-font registration, and vector drawing primitives.
 */

#include <algorithm>
#include <array>
#include <cerrno>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pgl::pdfgen {

struct pdf_doc;
struct pdf_object;

struct pdf_info {
    char creator[64]{};
    char producer[64]{};
    char title[64]{};
    char author[64]{};
    char subject[64]{};
    char date[64]{};
};

struct pdf_path_operation {
    char op{};
    float x1{};
    float y1{};
    float x2{};
    float y2{};
    float x3{};
    float y3{};
};

inline constexpr float PDF_INCH_TO_POINT(float inch) {
    return inch * 72.0f;
}

inline constexpr float PDF_MM_TO_POINT(float mm) {
    return mm * 72.0f / 25.4f;
}

inline constexpr float PDF_LETTER_WIDTH = PDF_INCH_TO_POINT(8.5f);
inline constexpr float PDF_LETTER_HEIGHT = PDF_INCH_TO_POINT(11.0f);
inline constexpr float PDF_A4_WIDTH = PDF_MM_TO_POINT(210.0f);
inline constexpr float PDF_A4_HEIGHT = PDF_MM_TO_POINT(297.0f);
inline constexpr float PDF_A3_WIDTH = PDF_MM_TO_POINT(297.0f);
inline constexpr float PDF_A3_HEIGHT = PDF_MM_TO_POINT(420.0f);

inline constexpr std::uint32_t PDF_RGB(unsigned int r, unsigned int g, unsigned int b) {
    return (((r & 0xffu) << 16) | ((g & 0xffu) << 8) | (b & 0xffu));
}

inline constexpr std::uint32_t PDF_ARGB(unsigned int a, unsigned int r, unsigned int g, unsigned int b) {
    return (((a & 0xffu) << 24) | ((r & 0xffu) << 16) | ((g & 0xffu) << 8) | (b & 0xffu));
}

inline constexpr std::uint32_t PDF_RED = PDF_RGB(0xffu, 0u, 0u);
inline constexpr std::uint32_t PDF_GREEN = PDF_RGB(0u, 0xffu, 0u);
inline constexpr std::uint32_t PDF_BLUE = PDF_RGB(0u, 0u, 0xffu);
inline constexpr std::uint32_t PDF_BLACK = PDF_RGB(0u, 0u, 0u);
inline constexpr std::uint32_t PDF_WHITE = PDF_RGB(0xffu, 0xffu, 0xffu);
inline constexpr std::uint32_t PDF_TRANSPARENT = (0xffu << 24);

inline constexpr float PDF_RGB_R(std::uint32_t colour) {
    return static_cast<float>((colour >> 16) & 0xffu) / 255.0f;
}

inline constexpr float PDF_RGB_G(std::uint32_t colour) {
    return static_cast<float>((colour >> 8) & 0xffu) / 255.0f;
}

inline constexpr float PDF_RGB_B(std::uint32_t colour) {
    return static_cast<float>(colour & 0xffu) / 255.0f;
}

inline constexpr bool PDF_IS_TRANSPARENT(std::uint32_t colour) {
    return (colour >> 24) == 0xffu;
}

namespace detail {

enum {
    OBJ_none,
    OBJ_info,
    OBJ_stream,
    OBJ_font,
    OBJ_page,
    OBJ_ext_gstate,
    OBJ_catalog,
    OBJ_pages,
    OBJ_count,
};

inline constexpr std::array valid_fonts = {
    "Times-Roman",
    "Times-Bold",
    "Times-Italic",
    "Times-BoldItalic",
    "Helvetica",
    "Helvetica-Bold",
    "Helvetica-Oblique",
    "Helvetica-BoldOblique",
    "Courier",
    "Courier-Bold",
    "Courier-Oblique",
    "Courier-BoldOblique",
    "Symbol",
    "ZapfDingbats",
};

struct stream_data {
    pdf_object* page = nullptr;
    std::string stream;
};

struct page_data {
    float width = 0.0f;
    float height = 0.0f;
    std::vector<pdf_object*> children;
    std::vector<pdf_object*> ext_gstates;
};

struct font_data {
    char name[64]{};
    int index = 0;
    bool is_ttf = false;
};

struct ext_gstate_data {
    float fill_alpha = 1.0f;
    float stroke_alpha = 1.0f;
};

inline int ascii_casecmp(std::string_view left, std::string_view right) {
    const std::size_t size = std::min(left.size(), right.size());
    for (std::size_t i = 0; i < size; ++i) {
        const unsigned char a = static_cast<unsigned char>(left[i]);
        const unsigned char b = static_cast<unsigned char>(right[i]);
        const int lower_a = (a >= 'A' && a <= 'Z') ? (a - 'A' + 'a') : a;
        const int lower_b = (b >= 'A' && b <= 'Z') ? (b - 'A' + 'a') : b;
        if (lower_a != lower_b) {
            return lower_a - lower_b;
        }
    }
    if (left.size() == right.size()) {
        return 0;
    }
    return left.size() < right.size() ? -1 : 1;
}

inline void append_format(std::string& out, const char* format, ...) {
    va_list args;
    va_start(args, format);
    va_list copy;
    va_copy(copy, args);
    const int required = std::vsnprintf(nullptr, 0, format, copy);
    va_end(copy);
    if (required < 0) {
        va_end(args);
        return;
    }
    const std::size_t old_size = out.size();
    out.resize(old_size + static_cast<std::size_t>(required) + 1);
    std::vsnprintf(out.data() + old_size, static_cast<std::size_t>(required) + 1, format, args);
    out.resize(old_size + static_cast<std::size_t>(required));
    va_end(args);
}

inline void append_escaped_string(std::string& out, std::string_view value) {
    out.push_back('(');
    for (const char character : value) {
        if (character == '(' || character == ')' || character == '\\') {
            out.push_back('\\');
        }
        out.push_back(character);
    }
    out.push_back(')');
}

inline std::string now_as_pdf_date() {
    std::time_t now = std::time(nullptr);
    std::tm tm{};
#if defined(_WIN32)
    const std::tm* tmp = std::localtime(&now);
    if (tmp != nullptr) {
        tm = *tmp;
    }
#else
    localtime_r(&now, &tm);
#endif
    char buffer[64]{};
    std::strftime(buffer, sizeof(buffer), "%Y%m%d%H%M%SZ", &tm);
    return buffer;
}

} // namespace detail

struct pdf_object {
    int type = detail::OBJ_none;
    int index = 0;
    std::size_t offset = 0;
    pdf_object* prev = nullptr;
    pdf_object* next = nullptr;
    pdf_info info{};
    detail::stream_data stream{};
    detail::page_data page{};
    detail::font_data font{};
    detail::ext_gstate_data ext_gstate{};
};

struct pdf_doc {
    char errstr[128]{};
    int errval = 0;
    std::vector<std::unique_ptr<pdf_object>> objects;
    float width = 0.0f;
    float height = 0.0f;
    pdf_object* current_font = nullptr;
    std::array<pdf_object*, detail::OBJ_count> last_objects{};
    std::array<pdf_object*, detail::OBJ_count> first_objects{};
};

inline int pdf_set_err(pdf_doc* doc, int errval, const char* buffer, ...) {
    if (doc == nullptr) {
        return errval;
    }
    va_list args;
    va_start(args, buffer);
    const int len = std::vsnprintf(doc->errstr, sizeof(doc->errstr), buffer, args);
    va_end(args);
    if (len < 0) {
        doc->errstr[0] = '\0';
    } else {
        doc->errstr[sizeof(doc->errstr) - 1] = '\0';
    }
    doc->errval = errval;
    return errval;
}

inline const char* pdf_get_err(const pdf_doc* pdf, int* errval) {
    if (pdf == nullptr || pdf->errstr[0] == '\0') {
        return nullptr;
    }
    if (errval != nullptr) {
        *errval = pdf->errval;
    }
    return pdf->errstr;
}

inline void pdf_clear_err(pdf_doc* pdf) {
    if (pdf == nullptr) {
        return;
    }
    pdf->errstr[0] = '\0';
    pdf->errval = 0;
}

inline pdf_object* pdf_get_object(const pdf_doc* pdf, int index) {
    if (pdf == nullptr || index < 0 || static_cast<std::size_t>(index) >= pdf->objects.size()) {
        return nullptr;
    }
    return pdf->objects[static_cast<std::size_t>(index)].get();
}

inline pdf_object* pdf_find_first_object(const pdf_doc* pdf, int type) {
    return pdf == nullptr ? nullptr : pdf->first_objects[static_cast<std::size_t>(type)];
}

inline pdf_object* pdf_find_last_object(const pdf_doc* pdf, int type) {
    return pdf == nullptr ? nullptr : pdf->last_objects[static_cast<std::size_t>(type)];
}

inline int pdf_append_object(pdf_doc* pdf, pdf_object* object) {
    if (pdf == nullptr || object == nullptr) {
        return -EINVAL;
    }
    object->index = static_cast<int>(pdf->objects.size());
    if (pdf->last_objects[static_cast<std::size_t>(object->type)] != nullptr) {
        object->prev = pdf->last_objects[static_cast<std::size_t>(object->type)];
        object->prev->next = object;
    }
    pdf->last_objects[static_cast<std::size_t>(object->type)] = object;
    if (pdf->first_objects[static_cast<std::size_t>(object->type)] == nullptr) {
        pdf->first_objects[static_cast<std::size_t>(object->type)] = object;
    }
    return 0;
}

inline pdf_object* pdf_add_object(pdf_doc* pdf, int type) {
    if (pdf == nullptr) {
        return nullptr;
    }
    std::unique_ptr<pdf_object> object = std::make_unique<pdf_object>();
    object->type = type;
    pdf_object* raw = object.get();
    if (pdf_append_object(pdf, raw) < 0) {
        return nullptr;
    }
    pdf->objects.push_back(std::move(object));
    return raw;
}

inline float pdf_width(const pdf_doc* pdf) {
    return pdf == nullptr ? 0.0f : pdf->width;
}

inline float pdf_height(const pdf_doc* pdf) {
    return pdf == nullptr ? 0.0f : pdf->height;
}

inline float pdf_page_width(const pdf_object* page) {
    return page == nullptr ? 0.0f : page->page.width;
}

inline float pdf_page_height(const pdf_object* page) {
    return page == nullptr ? 0.0f : page->page.height;
}

inline int pdf_set_font(pdf_doc* pdf, const char* font) {
    if (pdf == nullptr || font == nullptr) {
        return pdf_set_err(pdf, -EINVAL, "Invalid font");
    }

    const char* canonical = nullptr;
    for (const char* candidate : detail::valid_fonts) {
        if (detail::ascii_casecmp(font, candidate) == 0) {
            canonical = candidate;
            break;
        }
    }
    if (canonical == nullptr) {
        return pdf_set_err(pdf, -EINVAL, "Invalid font name '%s'", font);
    }

    int last_index = 0;
    for (pdf_object* object = pdf_find_first_object(pdf, detail::OBJ_font); object != nullptr; object = object->next) {
        last_index = std::max(last_index, object->font.index);
        if (std::strcmp(object->font.name, canonical) == 0) {
            pdf->current_font = object;
            return 0;
        }
    }

    pdf_object* object = pdf_add_object(pdf, detail::OBJ_font);
    if (object == nullptr) {
        return pdf_set_err(pdf, -ENOMEM, "Unable to allocate PDF font");
    }

    std::snprintf(object->font.name, sizeof(object->font.name), "%s", canonical);
    object->font.name[sizeof(object->font.name) - 1] = '\0';
    object->font.index = last_index + 1;
    pdf->current_font = object;
    return 0;
}

inline pdf_doc* pdf_create(float width, float height, const pdf_info* info) {
    std::unique_ptr<pdf_doc> pdf = std::make_unique<pdf_doc>();
    pdf->width = width;
    pdf->height = height;

    (void)pdf_add_object(pdf.get(), detail::OBJ_none);

    pdf_object* info_object = pdf_add_object(pdf.get(), detail::OBJ_info);
    if (info_object == nullptr) {
        return nullptr;
    }
    if (info != nullptr) {
        info_object->info = *info;
        info_object->info.creator[sizeof(info_object->info.creator) - 1] = '\0';
        info_object->info.producer[sizeof(info_object->info.producer) - 1] = '\0';
        info_object->info.title[sizeof(info_object->info.title) - 1] = '\0';
        info_object->info.author[sizeof(info_object->info.author) - 1] = '\0';
        info_object->info.subject[sizeof(info_object->info.subject) - 1] = '\0';
        info_object->info.date[sizeof(info_object->info.date) - 1] = '\0';
    }
    if (info_object->info.date[0] == '\0') {
        const std::string date = detail::now_as_pdf_date();
        std::snprintf(info_object->info.date, sizeof(info_object->info.date), "%s", date.c_str());
    }

    if (pdf_add_object(pdf.get(), detail::OBJ_pages) == nullptr) {
        return nullptr;
    }
    if (pdf_add_object(pdf.get(), detail::OBJ_catalog) == nullptr) {
        return nullptr;
    }
    if (pdf_set_font(pdf.get(), "Times-Roman") < 0) {
        return nullptr;
    }

    return pdf.release();
}

inline void pdf_destroy(pdf_doc* pdf) {
    delete pdf;
}

inline pdf_object* pdf_append_page(pdf_doc* pdf) {
    pdf_object* page = pdf_add_object(pdf, detail::OBJ_page);
    if (page == nullptr) {
        return nullptr;
    }
    page->page.width = pdf->width;
    page->page.height = pdf->height;
    return page;
}

inline pdf_object* pdf_get_page(pdf_doc* pdf, int page_number) {
    if (page_number <= 0) {
        pdf_set_err(pdf, -EINVAL, "page number must be >= 1");
        return nullptr;
    }
    for (pdf_object* object = pdf_find_first_object(pdf, detail::OBJ_page); object != nullptr; object = object->next, --page_number) {
        if (page_number == 1) {
            return object;
        }
    }
    pdf_set_err(pdf, -EINVAL, "no such page");
    return nullptr;
}

inline int pdf_page_set_size(pdf_doc* pdf, pdf_object* page, float width, float height) {
    if (page == nullptr) {
        page = pdf_find_last_object(pdf, detail::OBJ_page);
    }
    if (page == nullptr || page->type != detail::OBJ_page) {
        return pdf_set_err(pdf, -EINVAL, "Invalid PDF page");
    }
    page->page.width = width;
    page->page.height = height;
    return 0;
}

inline int pdf_add_stream(pdf_doc* pdf, pdf_object* page, const std::string& buffer) {
    if (page == nullptr) {
        page = pdf_find_last_object(pdf, detail::OBJ_page);
    }
    if (page == nullptr || page->type != detail::OBJ_page) {
        return pdf_set_err(pdf, -EINVAL, "Invalid pdf page");
    }

    std::string trimmed = buffer;
    while (!trimmed.empty() && (trimmed.back() == '\r' || trimmed.back() == '\n')) {
        trimmed.pop_back();
    }

    pdf_object* object = pdf_add_object(pdf, detail::OBJ_stream);
    if (object == nullptr) {
        return pdf_set_err(pdf, -ENOMEM, "Unable to allocate PDF stream");
    }
    object->stream.page = page;
    object->stream.stream = std::move(trimmed);
    page->page.children.push_back(object);
    return 0;
}

inline std::string pdf_escape_content_string(std::string_view value) {
    std::string escaped;
    escaped.reserve(value.size() + 4);
    for (const char character : value) {
        if (character == '(' || character == ')' || character == '\\') {
            escaped.push_back('\\');
        }
        if (character == '\n' || character == '\r' || character == '\t' || character == '\b' || character == '\f') {
            continue;
        }
        escaped.push_back(character);
    }
    return escaped;
}

inline int pdf_add_text(pdf_doc* pdf, pdf_object* page, const char* text, float size, float xoff, float yoff, std::uint32_t colour) {
    if (text == nullptr || *text == '\0') {
        return 0;
    }
    if (pdf == nullptr || pdf->current_font == nullptr) {
        return pdf_set_err(pdf, -EINVAL, "No active font");
    }
    std::string stream;
    detail::append_format(stream, "BT %f %f TD /F%d %f Tf %f %f %f rg (%s) Tj ET",
        xoff,
        yoff,
        pdf->current_font->font.index,
        size,
        PDF_RGB_R(colour),
        PDF_RGB_G(colour),
        PDF_RGB_B(colour),
        pdf_escape_content_string(text).c_str());
    return pdf_add_stream(pdf, page, stream);
}

inline float pdf_clamp_alpha(float alpha) {
    if (!std::isfinite(alpha)) {
        return 1.0f;
    }
    return std::clamp(alpha, 0.0f, 1.0f);
}

inline bool pdf_alpha_equal(float left, float right) {
    return std::fabs(left - right) <= 0.000001f;
}

inline int pdf_find_or_create_ext_gstate(pdf_doc* pdf, pdf_object* page, float fill_alpha, float stroke_alpha) {
    if (page == nullptr) {
        page = pdf_find_last_object(pdf, detail::OBJ_page);
    }
    if (page == nullptr || page->type != detail::OBJ_page) {
        return pdf_set_err(pdf, -EINVAL, "Invalid pdf page");
    }

    const float clamped_fill_alpha = pdf_clamp_alpha(fill_alpha);
    const float clamped_stroke_alpha = pdf_clamp_alpha(stroke_alpha);
    for (std::size_t index = 0; index < page->page.ext_gstates.size(); ++index) {
        const pdf_object* ext_gstate = page->page.ext_gstates[index];
        if (ext_gstate != nullptr &&
            ext_gstate->type == detail::OBJ_ext_gstate &&
            pdf_alpha_equal(ext_gstate->ext_gstate.fill_alpha, clamped_fill_alpha) &&
            pdf_alpha_equal(ext_gstate->ext_gstate.stroke_alpha, clamped_stroke_alpha)) {
            return static_cast<int>(index);
        }
    }

    pdf_object* ext_gstate = pdf_add_object(pdf, detail::OBJ_ext_gstate);
    if (ext_gstate == nullptr) {
        return pdf_set_err(pdf, -ENOMEM, "Unable to allocate PDF ExtGState");
    }
    ext_gstate->ext_gstate.fill_alpha = clamped_fill_alpha;
    ext_gstate->ext_gstate.stroke_alpha = clamped_stroke_alpha;
    page->page.ext_gstates.push_back(ext_gstate);
    return static_cast<int>(page->page.ext_gstates.size() - 1);
}

inline int pdf_add_line_pattern(pdf_doc* pdf, pdf_object* page, float x1, float y1, float x2, float y2, float width,
    std::uint32_t colour, const float pattern[], int pattern_len, float phase, float stroke_alpha = 1.0f) {
    if (pattern_len < 0 || (pattern_len > 0 && pattern == nullptr)) {
        return pdf_set_err(pdf, -EINVAL, "Invalid line pattern");
    }
    if (width <= 0.0f || PDF_IS_TRANSPARENT(colour)) {
        return 0;
    }

    int ext_gstate_index = -1;
    const float effective_stroke_alpha = pdf_clamp_alpha(stroke_alpha);
    if (effective_stroke_alpha < 1.0f) {
        ext_gstate_index = pdf_find_or_create_ext_gstate(pdf, page, 1.0f, effective_stroke_alpha);
        if (ext_gstate_index < 0) {
            return ext_gstate_index;
        }
    }

    std::string stream;
    if (ext_gstate_index >= 0) {
        stream += "q\r\n";
    }
    if (pattern_len > 0) {
        stream.push_back('[');
        for (int i = 0; i < pattern_len; ++i) {
            detail::append_format(stream, "%s%f", i == 0 ? "" : " ", pattern[i]);
        }
        detail::append_format(stream, "] %f d\r\n", phase);
    }
    detail::append_format(stream, "%f w\r\n", width);
    detail::append_format(stream, "%f %f m\r\n", x1, y1);
    detail::append_format(stream, "/DeviceRGB CS\r\n%f %f %f RG\r\n", PDF_RGB_R(colour), PDF_RGB_G(colour), PDF_RGB_B(colour));
    detail::append_format(stream, "%f %f l\r\n", x2, y2);
    if (ext_gstate_index >= 0) {
        detail::append_format(stream, "/GS%d gs\r\n", ext_gstate_index);
    }
    stream += "S\r\n";
    if (pattern_len > 0) {
        stream += "[] 0 d\r\n";
    }
    if (ext_gstate_index >= 0) {
        stream += "Q\r\n";
    }
    return pdf_add_stream(pdf, page, stream);
}

inline int pdf_add_line(pdf_doc* pdf, pdf_object* page, float x1, float y1, float x2, float y2, float width, std::uint32_t colour,
    float stroke_alpha = 1.0f) {
    return pdf_add_line_pattern(pdf, page, x1, y1, x2, y2, width, colour, nullptr, 0, 0.0f, stroke_alpha);
}

inline int pdf_add_cubic_bezier(pdf_doc* pdf, pdf_object* page, float x1, float y1, float x2, float y2, float xq1,
    float yq1, float xq2, float yq2, float width, std::uint32_t colour, float stroke_alpha = 1.0f) {
    if (width <= 0.0f || PDF_IS_TRANSPARENT(colour)) {
        return 0;
    }

    int ext_gstate_index = -1;
    const float effective_stroke_alpha = pdf_clamp_alpha(stroke_alpha);
    if (effective_stroke_alpha < 1.0f) {
        ext_gstate_index = pdf_find_or_create_ext_gstate(pdf, page, 1.0f, effective_stroke_alpha);
        if (ext_gstate_index < 0) {
            return ext_gstate_index;
        }
    }

    std::string stream;
    if (ext_gstate_index >= 0) {
        stream += "q\r\n";
    }
    detail::append_format(stream, "%f w\r\n", width);
    detail::append_format(stream, "%f %f m\r\n", x1, y1);
    detail::append_format(stream, "/DeviceRGB CS\r\n%f %f %f RG\r\n", PDF_RGB_R(colour), PDF_RGB_G(colour), PDF_RGB_B(colour));
    detail::append_format(stream, "%f %f %f %f %f %f c\r\n", xq1, yq1, xq2, yq2, x2, y2);
    if (ext_gstate_index >= 0) {
        detail::append_format(stream, "/GS%d gs\r\n", ext_gstate_index);
    }
    stream += "S\r\n";
    if (ext_gstate_index >= 0) {
        stream += "Q\r\n";
    }
    return pdf_add_stream(pdf, page, stream);
}

inline int pdf_add_quadratic_bezier(pdf_doc* pdf, pdf_object* page, float x1, float y1, float x2, float y2,
    float xq1, float yq1, float width, std::uint32_t colour, float stroke_alpha = 1.0f) {
    const float xc1 = x1 + (xq1 - x1) * (2.0f / 3.0f);
    const float yc1 = y1 + (yq1 - y1) * (2.0f / 3.0f);
    const float xc2 = x2 + (xq1 - x2) * (2.0f / 3.0f);
    const float yc2 = y2 + (yq1 - y2) * (2.0f / 3.0f);
    return pdf_add_cubic_bezier(pdf, page, x1, y1, x2, y2, xc1, yc1, xc2, yc2, width, colour, stroke_alpha);
}

inline int pdf_add_custom_path(pdf_doc* pdf, pdf_object* page, const pdf_path_operation* operations,
    int operation_count, float stroke_width, std::uint32_t stroke_colour, std::uint32_t fill_colour,
    float fill_alpha = 1.0f, float stroke_alpha = 1.0f) {
    if (operation_count < 0 || (operation_count > 0 && operations == nullptr)) {
        return pdf_set_err(pdf, -EINVAL, "Invalid path operations");
    }

    const bool has_fill = !PDF_IS_TRANSPARENT(fill_colour);
    const bool has_stroke = !PDF_IS_TRANSPARENT(stroke_colour) && stroke_width > 0.0f;
    if (!has_fill && !has_stroke) {
        return 0;
    }

    int ext_gstate_index = -1;
    const float effective_fill_alpha = has_fill ? pdf_clamp_alpha(fill_alpha) : 1.0f;
    const float effective_stroke_alpha = has_stroke ? pdf_clamp_alpha(stroke_alpha) : 1.0f;
    if (effective_fill_alpha < 1.0f || effective_stroke_alpha < 1.0f) {
        ext_gstate_index = pdf_find_or_create_ext_gstate(pdf, page, effective_fill_alpha, effective_stroke_alpha);
        if (ext_gstate_index < 0) {
            return ext_gstate_index;
        }
    }

    std::string stream;
    if (ext_gstate_index >= 0) {
        stream += "q\r\n";
    }
    if (has_fill) {
        detail::append_format(stream, "/DeviceRGB cs\r\n%f %f %f rg\r\n", PDF_RGB_R(fill_colour), PDF_RGB_G(fill_colour), PDF_RGB_B(fill_colour));
    }
    if (has_stroke) {
        detail::append_format(stream, "%f w\r\n/DeviceRGB CS\r\n%f %f %f RG\r\n", stroke_width, PDF_RGB_R(stroke_colour), PDF_RGB_G(stroke_colour), PDF_RGB_B(stroke_colour));
    }

    for (int i = 0; i < operation_count; ++i) {
        const pdf_path_operation& operation = operations[i];
        switch (operation.op) {
            case 'm':
                detail::append_format(stream, "%f %f m\r\n", operation.x1, operation.y1);
                break;
            case 'l':
                detail::append_format(stream, "%f %f l\r\n", operation.x1, operation.y1);
                break;
            case 'c':
                detail::append_format(stream, "%f %f %f %f %f %f c\r\n", operation.x1, operation.y1, operation.x2, operation.y2, operation.x3, operation.y3);
                break;
            case 'v':
                detail::append_format(stream, "%f %f %f %f v\r\n", operation.x1, operation.y1, operation.x2, operation.y2);
                break;
            case 'y':
                detail::append_format(stream, "%f %f %f %f y\r\n", operation.x1, operation.y1, operation.x2, operation.y2);
                break;
            case 'h':
                stream += "h\r\n";
                break;
            default:
                return pdf_set_err(pdf, -EINVAL, "Invalid operation");
        }
    }

    if (ext_gstate_index >= 0) {
        detail::append_format(stream, "/GS%d gs\r\n", ext_gstate_index);
    }
    if (has_fill && has_stroke) {
        stream += "B\r\n";
    } else if (has_fill) {
        stream += "f\r\n";
    } else {
        stream += "S\r\n";
    }
    if (ext_gstate_index >= 0) {
        stream += "Q\r\n";
    }

    return pdf_add_stream(pdf, page, stream);
}

inline int pdf_add_ellipse(pdf_doc* pdf, pdf_object* page, float x, float y, float xradius, float yradius, float width,
    std::uint32_t colour, std::uint32_t fill_colour, float fill_alpha = 1.0f, float stroke_alpha = 1.0f) {
    const float lx = (4.0f / 3.0f) * static_cast<float>(std::sqrt(2.0) - 1.0) * xradius;
    const float ly = (4.0f / 3.0f) * static_cast<float>(std::sqrt(2.0) - 1.0) * yradius;

    std::array<pdf_path_operation, 5> operations{{
        {'m', x + xradius, y, 0.0f, 0.0f, 0.0f, 0.0f},
        {'c', x + xradius, y - ly, x + lx, y - yradius, x, y - yradius},
        {'c', x - lx, y - yradius, x - xradius, y - ly, x - xradius, y},
        {'c', x - xradius, y + ly, x - lx, y + yradius, x, y + yradius},
        {'c', x + lx, y + yradius, x + xradius, y + ly, x + xradius, y},
    }};
    return pdf_add_custom_path(
        pdf,
        page,
        operations.data(),
        static_cast<int>(operations.size()),
        width,
        colour,
        fill_colour,
        fill_alpha,
        stroke_alpha
    );
}

inline int pdf_add_circle(pdf_doc* pdf, pdf_object* page, float x, float y, float radius, float width,
    std::uint32_t colour, std::uint32_t fill_colour, float fill_alpha = 1.0f, float stroke_alpha = 1.0f) {
    return pdf_add_ellipse(pdf, page, x, y, radius, radius, width, colour, fill_colour, fill_alpha, stroke_alpha);
}

inline int pdf_add_rectangle(pdf_doc* pdf, pdf_object* page, float x, float y, float width, float height,
    float border_width, std::uint32_t colour, float stroke_alpha = 1.0f) {
    if (border_width <= 0.0f || PDF_IS_TRANSPARENT(colour)) {
        return 0;
    }

    int ext_gstate_index = -1;
    const float effective_stroke_alpha = pdf_clamp_alpha(stroke_alpha);
    if (effective_stroke_alpha < 1.0f) {
        ext_gstate_index = pdf_find_or_create_ext_gstate(pdf, page, 1.0f, effective_stroke_alpha);
        if (ext_gstate_index < 0) {
            return ext_gstate_index;
        }
    }

    std::string stream;
    if (ext_gstate_index >= 0) {
        stream += "q\r\n";
    }
    detail::append_format(stream, "/DeviceRGB CS\r\n%f %f %f RG\r\n", PDF_RGB_R(colour), PDF_RGB_G(colour), PDF_RGB_B(colour));
    detail::append_format(stream, "%f w\r\n%f %f %f %f re\r\n", border_width, x, y, width, height);
    if (ext_gstate_index >= 0) {
        detail::append_format(stream, "/GS%d gs\r\n", ext_gstate_index);
    }
    stream += "S\r\n";
    if (ext_gstate_index >= 0) {
        stream += "Q\r\n";
    }
    return pdf_add_stream(pdf, page, stream);
}

inline int pdf_add_filled_rectangle(pdf_doc* pdf, pdf_object* page, float x, float y, float width, float height,
    float border_width, std::uint32_t colour_fill, std::uint32_t colour_border, float fill_alpha = 1.0f, float stroke_alpha = 1.0f) {
    const bool has_fill = !PDF_IS_TRANSPARENT(colour_fill);
    const bool has_stroke = !PDF_IS_TRANSPARENT(colour_border) && border_width > 0.0f;
    if (!has_fill && !has_stroke) {
        return 0;
    }

    int ext_gstate_index = -1;
    const float effective_fill_alpha = has_fill ? pdf_clamp_alpha(fill_alpha) : 1.0f;
    const float effective_stroke_alpha = has_stroke ? pdf_clamp_alpha(stroke_alpha) : 1.0f;
    if (effective_fill_alpha < 1.0f || effective_stroke_alpha < 1.0f) {
        ext_gstate_index = pdf_find_or_create_ext_gstate(pdf, page, effective_fill_alpha, effective_stroke_alpha);
        if (ext_gstate_index < 0) {
            return ext_gstate_index;
        }
    }

    std::string stream;
    if (ext_gstate_index >= 0) {
        stream += "q\r\n";
    }
    if (has_fill) {
        detail::append_format(stream, "/DeviceRGB cs\r\n%f %f %f rg\r\n", PDF_RGB_R(colour_fill), PDF_RGB_G(colour_fill), PDF_RGB_B(colour_fill));
    }
    if (has_stroke) {
        detail::append_format(stream, "/DeviceRGB CS\r\n%f %f %f RG\r\n%f w\r\n", PDF_RGB_R(colour_border), PDF_RGB_G(colour_border), PDF_RGB_B(colour_border), border_width);
    }
    detail::append_format(stream, "%f %f %f %f re\r\n", x, y, width, height);
    if (ext_gstate_index >= 0) {
        detail::append_format(stream, "/GS%d gs\r\n", ext_gstate_index);
    }
    stream += has_fill && has_stroke ? "B\r\n" : (has_fill ? "f\r\n" : "S\r\n");
    if (ext_gstate_index >= 0) {
        stream += "Q\r\n";
    }
    return pdf_add_stream(pdf, page, stream);
}

inline int pdf_add_polygon(pdf_doc* pdf, pdf_object* page, const float x[], const float y[], int count,
    float border_width, std::uint32_t colour, float stroke_alpha = 1.0f) {
    if (count <= 0) {
        return 0;
    }
    std::vector<pdf_path_operation> operations;
    operations.reserve(static_cast<std::size_t>(count) + 1);
    operations.push_back({'m', x[0], y[0], 0.0f, 0.0f, 0.0f, 0.0f});
    for (int i = 1; i < count; ++i) {
        operations.push_back({'l', x[i], y[i], 0.0f, 0.0f, 0.0f, 0.0f});
    }
    operations.push_back({'h', 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f});
    return pdf_add_custom_path(
        pdf,
        page,
        operations.data(),
        static_cast<int>(operations.size()),
        border_width,
        colour,
        PDF_TRANSPARENT,
        1.0f,
        stroke_alpha
    );
}

inline int pdf_add_filled_polygon(pdf_doc* pdf, pdf_object* page, const float x[], const float y[], int count,
    float border_width, std::uint32_t colour, float fill_alpha = 1.0f, float stroke_alpha = 1.0f) {
    if (count <= 0) {
        return 0;
    }
    std::vector<pdf_path_operation> operations;
    operations.reserve(static_cast<std::size_t>(count) + 1);
    operations.push_back({'m', x[0], y[0], 0.0f, 0.0f, 0.0f, 0.0f});
    for (int i = 1; i < count; ++i) {
        operations.push_back({'l', x[i], y[i], 0.0f, 0.0f, 0.0f, 0.0f});
    }
    operations.push_back({'h', 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f});
    return pdf_add_custom_path(
        pdf,
        page,
        operations.data(),
        static_cast<int>(operations.size()),
        border_width,
        colour,
        colour,
        fill_alpha,
        stroke_alpha
    );
}

inline int pdf_save_buffer(pdf_doc* pdf, std::string& out) {
    if (pdf == nullptr) {
        return -EINVAL;
    }
    out.clear();
    out += (pdf_find_first_object(pdf, detail::OBJ_ext_gstate) != nullptr ? "%PDF-1.4\r\n%\xFF\xFF\xFF\xFF\r\n" : "%PDF-1.3\r\n%\xFF\xFF\xFF\xFF\r\n");

    const auto save_object = [&](pdf_object& object) {
        object.offset = out.size();
        detail::append_format(out, "%d 0 obj\r\n", object.index);
        switch (object.type) {
            case detail::OBJ_stream:
                detail::append_format(out, "<< /Length %zu >>\r\nstream\r\n", object.stream.stream.size());
                out += object.stream.stream;
                out += "\r\nendstream\r\n";
                break;
            case detail::OBJ_info:
                out += "<<\r\n";
                if (object.info.creator[0] != '\0') {
                    out += "  /Creator ";
                    detail::append_escaped_string(out, object.info.creator);
                    out += "\r\n";
                }
                if (object.info.producer[0] != '\0') {
                    out += "  /Producer ";
                    detail::append_escaped_string(out, object.info.producer);
                    out += "\r\n";
                }
                if (object.info.title[0] != '\0') {
                    out += "  /Title ";
                    detail::append_escaped_string(out, object.info.title);
                    out += "\r\n";
                }
                if (object.info.author[0] != '\0') {
                    out += "  /Author ";
                    detail::append_escaped_string(out, object.info.author);
                    out += "\r\n";
                }
                if (object.info.subject[0] != '\0') {
                    out += "  /Subject ";
                    detail::append_escaped_string(out, object.info.subject);
                    out += "\r\n";
                }
                if (object.info.date[0] != '\0') {
                    out += "  /CreationDate (D:";
                    out += object.info.date;
                    out += ")\r\n";
                }
                out += ">>\r\n";
                break;
            case detail::OBJ_page: {
                const pdf_object* pages = pdf_find_first_object(pdf, detail::OBJ_pages);
                out += "<<\r\n  /Type /Page\r\n";
                detail::append_format(out, "  /Parent %d 0 R\r\n", pages != nullptr ? pages->index : 0);
                detail::append_format(out, "  /MediaBox [0 0 %f %f]\r\n", object.page.width, object.page.height);
                out += "  /Resources <<\r\n    /Font <<\r\n";
                for (pdf_object* font = pdf_find_first_object(pdf, detail::OBJ_font); font != nullptr; font = font->next) {
                    detail::append_format(out, "      /F%d %d 0 R\r\n", font->font.index, font->index);
                }
                out += "    >>\r\n";
                if (!object.page.ext_gstates.empty()) {
                    out += "    /ExtGState <<\r\n";
                    for (std::size_t index = 0; index < object.page.ext_gstates.size(); ++index) {
                        const pdf_object* ext_gstate = object.page.ext_gstates[index];
                        detail::append_format(out, "      /GS%zu %d 0 R\r\n", index, ext_gstate != nullptr ? ext_gstate->index : 0);
                    }
                    out += "    >>\r\n";
                }
                out += "  >>\r\n  /Contents [\r\n";
                for (const pdf_object* child : object.page.children) {
                    detail::append_format(out, "%d 0 R\r\n", child->index);
                }
                out += "]\r\n>>\r\n";
                break;
            }
            case detail::OBJ_ext_gstate:
                detail::append_format(out,
                    "<<\r\n  /Type /ExtGState\r\n  /ca %f\r\n  /CA %f\r\n>>\r\n",
                    object.ext_gstate.fill_alpha,
                    object.ext_gstate.stroke_alpha);
                break;
            case detail::OBJ_font:
                detail::append_format(out,
                    "<<\r\n  /Type /Font\r\n  /Subtype /Type1\r\n  /BaseFont /%s\r\n  /Encoding /WinAnsiEncoding\r\n>>\r\n",
                    object.font.name);
                break;
            case detail::OBJ_pages: {
                out += "<<\r\n  /Type /Pages\r\n  /Kids [ ";
                int pages_count = 0;
                for (pdf_object* page = pdf_find_first_object(pdf, detail::OBJ_page); page != nullptr; page = page->next) {
                    ++pages_count;
                    detail::append_format(out, "%d 0 R ", page->index);
                }
                out += "]\r\n";
                detail::append_format(out, "  /Count %d\r\n>>\r\n", pages_count);
                break;
            }
            case detail::OBJ_catalog: {
                const pdf_object* pages = pdf_find_first_object(pdf, detail::OBJ_pages);
                out += "<<\r\n  /Type /Catalog\r\n";
                detail::append_format(out, "  /Pages %d 0 R\r\n>>\r\n", pages != nullptr ? pages->index : 0);
                break;
            }
            case detail::OBJ_none:
                out += "<<>>\r\n";
                break;
            default:
                break;
        }
        out += "endobj\r\n";
    };

    for (std::size_t index = 1; index < pdf->objects.size(); ++index) {
        pdf_object* object = pdf->objects[index].get();
        if (object != nullptr) {
            save_object(*object);
        }
    }

    const std::size_t xref_offset = out.size();
    detail::append_format(out, "xref\r\n0 %zu\r\n", pdf->objects.size());
    out += "0000000000 65535 f\r\n";
    for (std::size_t index = 1; index < pdf->objects.size(); ++index) {
        const pdf_object* object = pdf->objects[index].get();
        detail::append_format(out, "%010zu 00000 n\r\n", object != nullptr ? object->offset : static_cast<std::size_t>(0));
    }

    const pdf_object* info = pdf_find_first_object(pdf, detail::OBJ_info);
    const pdf_object* catalog = pdf_find_first_object(pdf, detail::OBJ_catalog);
    out += "trailer\r\n<<\r\n";
    detail::append_format(out, "  /Size %zu\r\n", pdf->objects.size());
    if (catalog != nullptr) {
        detail::append_format(out, "  /Root %d 0 R\r\n", catalog->index);
    }
    if (info != nullptr) {
        detail::append_format(out, "  /Info %d 0 R\r\n", info->index);
    }
    out += ">>\r\nstartxref\r\n";
    detail::append_format(out, "%zu\r\n%%%%EOF\r\n", xref_offset);
    return 0;
}

inline int pdf_save_file(pdf_doc* pdf, FILE* fp) {
    if (fp == nullptr) {
        return pdf_set_err(pdf, -EINVAL, "Invalid FILE pointer");
    }
    std::string buffer;
    const int result = pdf_save_buffer(pdf, buffer);
    if (result < 0) {
        return result;
    }
    if (!buffer.empty()) {
        const std::size_t written = std::fwrite(buffer.data(), 1, buffer.size(), fp);
        if (written != buffer.size()) {
            return pdf_set_err(pdf, -errno, "Unable to write PDF bytes");
        }
    }
    return 0;
}

inline int pdf_save(pdf_doc* pdf, const char* filename) {
    if (filename == nullptr) {
        return pdf_set_err(pdf, -EINVAL, "Invalid filename");
    }
    std::ofstream output(filename, std::ios::binary);
    if (!output) {
        return pdf_set_err(pdf, -errno, "Unable to open '%s'", filename);
    }
    std::string buffer;
    const int result = pdf_save_buffer(pdf, buffer);
    if (result < 0) {
        return result;
    }
    output.write(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    if (!output) {
        return pdf_set_err(pdf, -errno, "Unable to write '%s'", filename);
    }
    return 0;
}

inline std::string pdf_to_string(pdf_doc* pdf) {
    std::string buffer;
    if (pdf_save_buffer(pdf, buffer) < 0) {
        return {};
    }
    return buffer;
}

} // namespace pgl::pdfgen
