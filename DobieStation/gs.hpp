#ifndef GS_HPP
#define GS_HPP
#include <cstdint>

struct GIFtag
{
    uint16_t NLOOP;
    bool end_of_packet;
    bool output_PRIM;
    uint16_t PRIM;
    uint8_t format;
    uint8_t reg_count;
    uint64_t regs;

    uint8_t regs_left;
    uint32_t data_left;
};

struct PRIM_REG
{
    uint8_t prim_type;
    bool gourand_shading;
    bool texture_mapping;
    bool fog;
    bool alpha_blend;
    bool antialiasing;
    bool use_UV;
    bool use_context2;
    bool fix_fragment_value;
};

struct RGBAQ_REG
{
    uint8_t r, g, b, a;
    float q;
};

struct XYOFFSET
{
    uint16_t x;
    uint16_t y;
};

struct SCISSOR
{
    uint16_t x1, x2, y1, y2;
};

struct TEST
{
    bool alpha_test;
    uint8_t alpha_method;
    uint8_t alpha_ref;
    uint8_t alpha_fail_method;
    bool dest_alpha_test;
    bool dest_alpha_method;
    bool depth_test;
    uint8_t depth_method;
};

struct FRAME
{
    uint32_t base_pointer;
    uint32_t width;
    uint8_t format;
    uint32_t mask;
};

struct ZBUF
{
    uint32_t base_pointer;
    uint8_t format;
    bool no_update;
};

struct BITBLTBUF_REG
{
    uint32_t source_base;
    uint32_t source_width;
    uint8_t source_format;
    uint32_t dest_base;
    uint32_t dest_width;
    uint8_t dest_format;
};

struct TRXPOS_REG
{
    uint16_t source_x, source_y;
    uint16_t dest_x, dest_y;
    uint8_t trans_order;
};

//reg reg
struct TRXREG_REG
{
    uint16_t width;
    uint16_t height;
};

class GraphicsSynthesizer
{
    private:
        uint32_t* local_mem;

        GIFtag current_tag;
        bool processing_GIF_prim;

        PRIM_REG PRIM;
        RGBAQ_REG RGBAQ;
        XYOFFSET XYOFFSET_1, XYOFFSET_2;
        bool DTHE;
        bool COLCLAMP;
        bool use_PRIM;
        SCISSOR SCISSOR_1, SCISSOR_2;
        TEST TEST_1, TEST_2;
        FRAME FRAME_1, FRAME_2;
        ZBUF ZBUF_1, ZBUF_2;


        BITBLTBUF_REG BITBLTBUF;
        TRXPOS_REG TRXPOS;
        TRXREG_REG TRXREG;
        uint8_t TRXDIR;
        uint32_t transfer_addr;
        int transfer_bit_depth;
        int pixels_transferred;

        void process_PACKED(uint64_t data[2]);
        void write_HWREG(uint64_t data);
    public:
        GraphicsSynthesizer();
        ~GraphicsSynthesizer();
        void reset();

        uint32_t read32(uint32_t addr);
        void write32(uint32_t addr, uint32_t value);
        void write64(uint32_t addr, uint64_t value);

        void feed_GIF(uint64_t data[2]);
        void send_PATH3(uint64_t data[2]);
};

#endif // GS_HPP