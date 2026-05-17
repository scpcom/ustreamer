typedef struct {
    uint32_t par1; // if (from == 4) par1a = 0x101
    uint32_t ivps_grp; // IvpsGrp
    uint32_t ivps_chn; // IvpsChn
    uint16_t pipe_id; // nPipeId +2 = venc_ch
    uint16_t venc_chn;
    uint16_t vdec_chn; // vdec_ch
    uint16_t par5b;
    uint32_t card; // card
    uint32_t device; // device
    uint32_t raw_id; // eRawId
    uint32_t sns_frame; // eSnsFrame
    uint32_t par10;

    uint8_t data[0x330];

    uint32_t from;
    uint32_t pad;
} frame_param_t;

typedef struct {
    uint32_t par1; // if (from == 4) par1a = 0x101
    uint32_t ivps_grp; // IvpsGrp
    uint32_t ivps_chn; // IvpsChn
    uint16_t pipe_id; // nPipeId +2 = venc_ch
    uint16_t venc_chn;
    uint16_t vdec_chn; // vdec_ch
    uint16_t par5b;
    uint32_t card; // card
    uint32_t device; // device
    uint32_t raw_id; // eRawId
    uint32_t sns_frame; // eSnsFrame
    uint32_t par10;

    AX_VIDEO_FRAME_T stFrame;
    uint8_t data[0x248];

    uint32_t from;
    uint32_t pad;
} frame_video_param_t;

typedef struct {
    uint32_t par1; // if (from == 4) par1a = 0x101
    uint32_t ivps_grp; // IvpsGrp
    uint32_t ivps_chn; // IvpsChn
    uint16_t pipe_id; // nPipeId +2 = venc_ch
    uint16_t venc_chn;
    uint16_t vdec_chn; // vdec_ch
    uint16_t par5b;
    uint32_t card; // card
    uint32_t device; // device
    uint32_t raw_id; // eRawId
    uint32_t sns_frame; // eSnsFrame
    uint32_t par10;

    uint32_t bit_width;
    uint32_t sound_mode;
    void *vir_addr;
    void *phy_addr;
    uint64_t timestamp;
    uint32_t seq;
    uint32_t len;
    uint32_t pool_id[2];
    uint32_t eof;
    uint32_t blk_id;
    uint8_t data[0x2f8];

    uint32_t from;
    uint32_t pad;
} frame_audio_param_t;
