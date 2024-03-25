/**
 * @file barcode.h
 * @author Shen Qi (qi.shen@smartmore.com)
 * @author Chen Miaohui (mira.chan@smartmore.com)
 * @author Chen Siyuan (siyuan.chen@smartmore.com)
 * @brief
 * @version 0.29.4_rknn_test
 * @date 2024-03-15
 *
 * @copyright Copyright (c) 2021 SmartMore
 */

#ifndef SMARTMORE_BARCODE_H
#define SMARTMORE_BARCODE_H

#include <cstdint>
#include <memory>
#include <opencv2/core/base.hpp>
#include <opencv2/core/types.hpp>
#include <opencv2/opencv.hpp>
#include <ostream>
#include <vector>

#ifdef BUILD_WITH_ONED_BENCHMARK
#include <unordered_map>
#endif

/**
 * @brief namespace smartmore
 *
 */
namespace smartmore
{
/**
 * @brief namespace smartmore::barcode
 *
 */
namespace barcode
{
using Roi = std::array<cv::Point2f, 4>;

/**
 * @brief struct ISOStandard
 * @struct ISOStandard
 */
enum class ISOStandard : uint32_t
{
    kISO1541x = 0, ///< quality based on ISO15415/6 (1D,DM, QR)
    kISO29158 = 1  ///< quality based on ISO29158 (DM, QR)
};

/**
 * @brief struct BarcodeParams2DQuota
 * @struct BarcodeParams2DQuota
 */
struct BarcodeParams2DQuota
{
    // iso15415
    std::vector<float> decode_thres = {};
    std::vector<float> sc_thres = {0.70f, 0.55f, 0.40f, 0.20};
    std::vector<float> mod_thres = {0.50f, 0.40f, 0.30f, 0.20};
    std::vector<float> an_thres = {0.94, 0.92, 0.90, 0.88};
    std::vector<float> gn_thres = {0.62, 0.45, 0.37, 0.25};
    // std::vector<float> an_thres = {0.06, 0.08, 0.10, 0.12};
    // std::vector<float> gn_thres = {0.38, 0.50, 0.63, 0.75};
    std::vector<float> uec_thres = {0.62, 0.50, 0.45, 0.25};
    std::vector<float> fpd_thres = {0.85, 0.80, 0.75, 0.50};
    std::vector<float> print_growth_thres = {0.5, 0.3, 0.15, 0.0};

    bool decode_enable = true;
    bool sc_enable = true;
    bool mod_enable = true;
    bool an_enable = true;
    bool gn_enable = true;
    bool uec_enable = true;
    bool fpd_enable = true;

    // iso29158
    bool cc_enable = true;
    bool cm_enable = true;
    bool rcal_enable = true;
    bool ddg_enable = true;
    std::vector<float> cc_thres = {0.3, 0.25, 0.20, 0.15};

    // only first and last were used, 0.05~4-0
    std::vector<float> rcal_thres = {0.05, 0.05, 0.05, 0.05};

    // not used yet
    std::vector<float> cm_mod_uec = {0.50f, 0.40f, 0.40f, 0.40f};

    // TODO(qhw): read from config file
    bool print_growth_enable = true;
    bool include_print_growth_in_final_grade = true;
};

/**
 * @brief struct BarcodeParams1DQuota
 * @struct BarcodeParams1DQuota
 */
struct BarcodeParams1DQuota
{
    std::vector<float> rmin_thres = {0.50f};
    std::vector<float> sc_thres = {0.55f, 0.40f, 0.20f, 0.10f};
    std::vector<float> ecmin_thres = {0.15f};
    std::vector<float> mod_thres = {0.35f, 0.30f, 0.25f, 0.20f};
    std::vector<float> defects_thres = {0.85f, 0.80f, 0.75f, 0.70f};
    std::vector<float> decodability_thres = {0.62f, 0.50f, 0.37f, 0.25f};

    bool rmin_enable = true;
    bool sc_enable = true;
    bool ecmin_enable = true;
    bool mod_enable = false;
    bool defects_enable = true;
    bool decodability_enable = true;
};

/**
 * @brief enum smartmore::Barcode::BarcodeType
 *
 */
enum class BarcodeType : uint32_t
{
    kUnknown = 0, // Unkown Code
    kCode39 = 1,  // Code 39 1D format.
    kCode93 = 2,  // Code 93 1D format.
    kCode128 = 4, // Code 128 1D format.
    kEan8 = 8,    // EAN-8 1D format.
    kEan13 = 16,  // EAN-13 1D format.
    kItf = 32,    // ITF (Interleaved Two of Five) 1D format.
    kUpcA = 64,   // UPC-A 1D format.
    kUpcE = 128,  // UPC-E 1D format.
    kUpcEanExtension = 256,
    // UPC/EAN extension format. Not a stand-alone format.
    kQrcode = 512,
    kDmcode = 1024,
    kPharmacode = 2048,
    kPdf417 = 4096,
    kUpcEan = smartmore::barcode::BarcodeType::kEan8 |
              smartmore::barcode::BarcodeType::kEan13 |
              smartmore::barcode::BarcodeType::kUpcA |
              smartmore::barcode::BarcodeType::kUpcE,
    kCode1D = smartmore::barcode::BarcodeType::kCode39 |
              smartmore::barcode::BarcodeType::kCode93 |
              smartmore::barcode::BarcodeType::kCode128 |
              smartmore::barcode::BarcodeType::kEan8 |
              smartmore::barcode::BarcodeType::kEan13 |
              smartmore::barcode::BarcodeType::kItf |
              smartmore::barcode::BarcodeType::kUpcA |
              smartmore::barcode::BarcodeType::kUpcE |
              smartmore::barcode::BarcodeType::kUpcEanExtension |
              smartmore::barcode::BarcodeType::kPharmacode,
    kCode2D =
        BarcodeType::kQrcode | BarcodeType::kDmcode | BarcodeType::kPdf417,
};

/**
 * @brief Image process operator
 *
 */
enum class ImgProcOp : uint32_t
{
    kDilate = 0,
    kErode = 1,
    kOpen = 2,
    kClose = 4,
    kMean = 8,
    kMedian = 16,
    kNormalize = 32,
    kUnsharpMask = 64
};

/**
 * @brief Barcode polarity state
 *
 */
enum class Polarity : uint32_t
{
    kAuto = 0,
    kDarkOnLight = 1,
    kLightOnDark = 2
};

/**
 * @brief Barcode mirror state
 *
 */
enum class MirrorType : uint32_t
{
    kAuto = 0,
    kNoMirror = 1,
    kMirrored = 2
};

/**
 * @brief Barcode running mode
 * @todo implementation
 */
enum class RunningMode : uint32_t
{
    kAuto = 0,
    kEfficiency = 1, // detection speed first
    kPerformance = 2 // detection quality first
};

/**
 * @brief edge type of dm code
 * @todo implementation
 */
enum class DMEdgeType : uint32_t
{
    kAuto = 0,
    kNoEdgeMode = 1,
    kEdgeMode = 2
};

/**
 * @brief shape type of dm code
 * @todo implementation
 */
enum class DMShapeType : uint32_t
{
    kAuto = 0,
    kSquare = 1,
    kRectangle = 2
};

/**
 * @brief ecc type of dm code
 *
 */
enum class DMECCType : uint32_t
{
    kAuto = 0,
    kECC000 = 1,
    kECC050 = 2,
    kECC080 = 3,
    kECC100 = 4,
    kECC140 = 5,
    kECC200 = 6
};

/**
 * @brief model type of qr code
 *
 */
enum class QRModelType : uint32_t
{
    kAuto = 0,
    kModel_1 = 1, // NOLINT
    kModel_2 = 2, // NOLINT
};

/**
 * @brief mode type of handheld
 *
 */
enum class HandheldModeType : uint32_t
{
    kUnSet = 0,
    kFull = 1,
    kCenter = 2,
    kCenterROI = 3
};

/**
 * @brief Bitwise or operator
 * @return BarcodeType
 */
inline BarcodeType operator|(BarcodeType a, BarcodeType b)
{
    return static_cast<BarcodeType>(static_cast<uint32_t>(a) |
                                    static_cast<uint32_t>(b));
}

/**
 * @brief Bitwise and operator
 * @return BarcodeType
 */
inline BarcodeType operator&(BarcodeType a, BarcodeType b)
{
    return static_cast<BarcodeType>(static_cast<uint32_t>(a) &
                                    static_cast<uint32_t>(b));
}

/**
 * @brief Judge BarcodeType a and BarcodeType b is intersected
 * @return bool
 */
inline bool IsIntersect(BarcodeType a, BarcodeType b)
{
    return (a & b) != BarcodeType::kUnknown;
}

inline std::ostream &operator<<(std::ostream &os, Polarity polarity)
{
    os << static_cast<uint32_t>(polarity);

    return os;
}

/**
 * @brief struct BarcodeRequest
 * @struct BarcodeRequest
 */
struct BarcodeRequest
{
    cv::Mat image; ///< input image (CV_8UC1)
    std::vector<std::array<cv::Point2f, 4>>
        key_points;                 ///< input key points for dl decode
    std::vector<BarcodeType> types; ///< input barcode type for dl decode

    BarcodeRequest(){};

    // NOLINTNEXTLINE
    BarcodeRequest(const cv::Mat &input_image) : image(input_image) {}

    BarcodeRequest(const cv::Mat &input_image,
                   const std::vector<std::array<cv::Point2f, 4>> &corners,
                   const std::vector<BarcodeType> &input_types)
        : image(input_image), key_points(corners), types(input_types)
    {
    }

    BarcodeRequest(uchar *data, int cols, int rows)
    {
        image = cv::Mat(rows, cols, CV_8UC1, data);
    }
};

/**
 * @brief struct ISO15416Quality
 * @struct ISO15416Quality
 */
struct ISO15416Quality
{
    int rmin;         ///< rmin
    int sc;           ///< sc
    int ecmin;        ///< ecmin
    int mod;          ///< mod
    int defects;      ///< defects
    int decodability; ///< decodability
    int overall;      ///< overall

    float rmin_raw;         ///< rmin raw value
    float rmax_raw;         ///< rmin raw value
    float rmin_rmax_raw;    ///< rmin/rmax raw value
    float sc_raw;           ///< sc raw value
    float ecmin_raw;        ///< ecmin raw value
    float mod_raw;          ///< mod raw value
    float defects_raw;      ///< defects raw value
    float decodability_raw; ///< decodability raw value
    ISO15416Quality()
        : rmin(-1), ecmin(-1), sc(-1), mod(-1), defects(-1), decodability(-1),
          overall(-1), rmin_raw(0), rmax_raw(0), sc_raw(0), ecmin_raw(0),
          mod_raw(0), defects_raw(0), decodability_raw(0)
    {
    }
};

/**
 * @brief struct ISO15415Quality
 * @struct ISO15415Quality
 */
struct ISO15415Quality
{
    /* Quality Grades(discrete 4, 3, 2, 1, 0) */
    int overall; ///< overall
    int decode;  ///< decode
    int sc;      ///< Symbol Contrast
    int mod;     ///< Modulation
    int an;      ///< Axial Non-uniformity
    int gn;      ///< Grid Non-uniformity
    int uec;     ///< UnusedErrorCorrection
    int fpd;     ///< Fixed Pattern Damage

    /* print growth  */
    int print_growth = -1;

    /* Quality Values(continuous value) */
    float r_min_value; ///< Reflect Min value
    float r_max_value; ///< Reflect Max value
    float gt_value;    ///< Global Threshold value
    float sc_value;    ///< Symbol Contrast value
    float mod_value;   ///< Modulation value
    float an_value;    ///< Axial Non-uniformity value
    float gn_value;    ///< Grid Non-uniformity value
    float uec_value;   ///< Unused Error Correction value
    float fpd_value;   ///< Fixed Pattern Damage value

    /* print growth in code horizontal direction */
    float print_growth_hori_value = 999;

    /* print growth in code vertical direction */
    float print_growth_vert_value = 999;

    ISO15415Quality()
        : overall(-1), decode(-1), sc(-1), mod(-1), an(-1), gn(-1), uec(-1),
          fpd(-1), r_min_value(-1), r_max_value(-1), gt_value(-1), sc_value(-1),
          mod_value(-1), an_value(-1), gn_value(-1), uec_value(-1),
          fpd_value(-1)
    {
    }
};

/**
 * @brief struct ISO29158Quality
 * @struct ISO29158Quality
 */
struct ISO29158Quality
{
    /* Quality Grades(discrete 4, 3, 2, 1, 0) */
    int overall;  ///< overall
    int decode;   ///< decode
    int cc;       ///< Cell Contrast
    int cm;       ///< CellModulation
    int r_target; ///< % reflectance of symbol rtarget
    int an;       ///< Axial Non-uniformity
    int gn;       ///< Grid Non-uniformity
    int uec;      ///< UnusedErrorCorrection
    int ddg;      ///< Distributed Damage Grade

    /* print growth  */
    int print_growth = -1;

    /* Quality Values(continuous value) */
    float cc_value;       ///< Cell Contrast value
    float cm_value;       ///< Cell Modulation value
    float r_target_value; ///< % reflectance of symbol
    float an_value;       ///< Axial Non-uniformity value
    float gn_value;       ///< Grid Non-uniformity value
    float uec_value;      ///< Unused Error Correction value
    float ddg_value;      ///< Distributed Damage Grade value

    /* print growth in code horizontal direction */
    float print_growth_hori_value = 999;
    /* print growth in code vertical direction */
    float print_growth_vert_value = 999;

    ISO29158Quality()
        : overall(-1), decode(-1), cc(-1), cm(-1), r_target(-1), ddg(-1),
          an(-1), gn(-1), uec(-1), cc_value(-1), cm_value(-1),
          r_target_value(-1), an_value(-1), gn_value(-1), uec_value(-1),
          ddg_value(-1)
    {
    }
};

/**
 * @brief struct BarcodeInfo
 * @struct BarcodeInfo
 */
struct BarcodeInfo
{
    int idx;                           ///< input roi idx
    bool detect_succeed;               ///< detect status
    bool decode_succeed;               ///< decode status
    bool succeed;                      ///< output status
    BarcodeType type;                  ///< barcode type
    std::string result;                ///< barcode info
    float ppm;                         ///< ppm
    MirrorType mirror;                 ///< mirror type
    Polarity polarity;                 ///< polarity type
    std::vector<cv::Point2f> grid_pts; ///< decode gird_pts
#ifdef BUILD_WITH_ANNOTATION
    std::vector<int> decode_bits; ///< decode bits
#endif
    float uec_value;                         ///< uec value
    RunningMode running_mode;                ///< running mode
    cv::RotatedRect rect;                    ///< min area rect
    std::array<cv::Point2f, 4> corner_boxes; ///< four corner points
    cv::Point2f center;                      ///< center point
    float barcode_angle_deg;                 ///< barcode angle degree
    std::array<int, 2> version;              ///< for twod barcode only
    std::array<float, 2> start_end_ratio;    ///< for oned barcode only
    std::string ec_level = ""; ///< for QR only, one of ("L", "M", "H", "Q")
    ISO15416Quality iso15416_quality; ///< quality based on ISO15416 (Barcode)
    ISO15415Quality iso15415_quality; ///< quality based on ISO15415 (DM, QR)
    ISO29158Quality iso29158_quality; ///< quality based on ISO29158 (DM, QR)
    float decode_time;                ///< barcode decode time(only dl)
    std::string raw;                  ///< decode raw (only oned)
    std::array<cv::Point2f, 4>
        coarse_locate_corner_boxes; ///< four dl coarse locate
                                    ///< corner_boxes (only dl)
    std::array<cv::Point2f, 4>
        precise_locate_corner_boxes; ///< four dl precise locate
                                     ///< corner_boxes (only dl)

#ifdef BUILD_WITH_ONED_BENCHMARK
    // use to output debug images
    std::unordered_map<std::string, cv::Mat> debug_images;
#endif // BUILD_WITH_ONED_BENCHMARK

    BarcodeInfo()
        : idx(0), detect_succeed(false), decode_succeed(false), succeed(false),
          type(BarcodeType::kUnknown), result(""), ppm(0),
          mirror(MirrorType::kAuto), polarity(Polarity::kAuto),
          barcode_angle_deg(0.f), version({0, 0}), decode_time(0.f), raw("")
    {
    }
};

/**
 * @brief struct BarcodeResponse
 * @struct BarcodeResponse
 */
struct BarcodeResponse
{
    unsigned int decode_1d;           ///< decode 1d count
    unsigned int decode_2d;           ///< decode 2d count
    std::vector<BarcodeInfo> results; ///< vector<BarcodeInfo>

    BarcodeResponse() : decode_1d(0), decode_2d(0) {}
    void Clear();
};

/**
 * @brief class BarcodeImpl
 *
 */
class BarcodeImpl;

/**
 * @brief class Barcode
 *
 * type              params barcode type
 *
 * timeout_period    params timeout period (unit ms), if
                     timeout_period == 0, not enable
 *
 * dl_enable         params dl enable
 *
 * twod_polarity     params twod polarity type, dark-on-light or light-on-dark
 *
 * twod_mirror_type  params twod mirror type
 *
 * twod_running_mode       params running mode, speed first or quality first
 *
 * twod_nums_output_max    params num of two dimensional code
 *
 * qr_distortion_enable    params for qr distortion enable
 *
 * qr_model_type           params for qr model type
 *
 * dm_edge_type            params for edge type dm
 *
 * dm_shape_type           params dm shape type, square or rectangle
 *
 * img_filters             params image filters vector
 *
 * oned_polarity           params oned polarity type, dark-on-light or
 *                         light-on-dark
 *
 * oned_nums_output_max    params num of one dimensional code
 *
 * oned_enable_full_scan   params enable full scan
 *
 * oned_min_ppm            params oned allowed min ppm
 *
 * oned_enable_curved_code      params curved code detect enable
 *
 * oned prelocate scale    params prelocate scale, valid range is 0.2~1.0,
 *                         if out of range, disable prelocate
 *
 * oned pyramid level      params pyramid level
 *
 * code39_enable_check_digit    params code39 check enable
 *
 * itf_enable_check_digit    params itf check enable
 */
class Barcode
{
public:
    /**
     * @brief construct function
     * params              |default                |type
     * type                |BarcodeType::kUnknown  |BarcodeType
     * timeout_period      |0                      |unsigned int
     * dl_enable           |false                  |bool
     * twod_polarity       |Polarity::kAuto        |Polarity
     * twod_mirror_type    |MirrorType::kAuto      |MirrorType
     * twod_running_mode       |RunningMode::kAuto     |RunningMode
     * twod_nums_output_max    |1                      |unsigned int
     * qr_distortion_enable    |true                   |bool
     * qr_model_type           |QRModelType::kModel_2  |QRModelType
     * dm_edge_type            |DMEdgeType::kAuto      |DMEdgeType
     * dm_shape_type           |DMShapeType::kAuto     |DMShapeType
     * img_filters             |[]                     |std::vector<ImgProcOp>
     * oned_polarity           |Polarity::kAuto        |Polarity
     * oned_running_mode       |RunningMode::kAuto     |RunningMode
     * oned_nums_output_max         |1                 |unsigned int
     * code39_enable_check_digit    |false             |bool
     * itf_enable_check_digit       |false             |bool
     *
     * @param config_dir <std::string> default config dir
     */
    explicit Barcode(const std::string &config_dir);

    /**
     * @brief default construct function
     */
    Barcode();

    /**
     * @brief deconstruct function
     */
    ~Barcode();

    /**
     * @brief get sdk version information
     * @return sdk version string
     */
    std::string GetVersion() const;

    /**
     * @brief decode image
     * @param input {input_image}
     * @param output vector({barcode_type, result_string, rotate_rect})
     * @return run status, 1 success 0 fail
     */
    bool Run(const BarcodeRequest &input, BarcodeResponse &output);

    /**
     * @brief decode image
     * @param input {{input_image}}
     * @param output {vector({barcode_type, result_string, rotate_rect}})
     * @return run status for each image
     */
    std::vector<bool> Run(const std::vector<BarcodeRequest> &input,
                          std::vector<BarcodeResponse> &output);
    /**
     * @brief load config
     * @param config_path <std::string> param config json file path
     */
    void LoadConfig(const std::string &config_path);

    /**
     * @brief unload config
     */
    void UnLoadConfig();

    /**
     * @brief save config
     * @param config_path <std::string> param config json file path
     */
    void SaveConfig(const std::string &config_path);

#if defined(__ANDROID__) || defined(__APPLE__)
    /**
     * @brief load license config
     * @param config_path <std::string> param config json file path
     */
    void LoadLicenseConfig(const std::string &config_path);
#endif
    /**
     * @brief set barcode type
     * @param type <BarcodeType>
     */
    void SetBarcodeType(BarcodeType type);

    /**
     * @brief get barcode type
     * @return BarcodeType
     */
    BarcodeType GetBarcodeType() const;

    /**
     * @brief (unit ms) set timeout period, if timeout_period == 0, not enable
     * @param timeout <int> [0, inf)
     */
    void SetTimeoutPeriod(unsigned int timeout);

    /**
     * @brief get timeout period
     * @return int
     */
    unsigned int GetTimeoutPeriod() const;

    /**
     * @brief set deep learning enable signal
     * @param value
     */
    void SetDLEnable(bool value);

    /**
     * @brief get deep learning enable signal
     * @return bool
     */
    bool GetDLEnable() const;

    /**
     * @brief set 1d deep learning enable signal
     * @param value
     * @return bool success:true, fail:false
     */
    bool Set1DDLEnable(bool value);

    /**
     * @brief get deep learning enable signal
     * @return bool
     */
    bool Get1DDLEnable() const;

    /**
     * @brief set polarity type
     * @param type Polarity::kxx
     */
    void SetPolarity2D(Polarity type);

    /**
     * @brief get polarity type
     * @return int
     */
    Polarity GetPolarity2D() const;

    /**
     * @brief set mirror type
     * @param type MirrorType::kxx
     */
    void SetMirrorType2D(MirrorType type);

    /**
     * @brief get mirror type
     * @return int
     */
    MirrorType GetMirrorType2D() const;

    /**
     * @brief Set running mode
     * @param running_mode RunningMode::kxx
     * @todo implementation
     */
    void SetRunningMode2D(RunningMode running_mode);

    /**
     * @brief Get running mode
     * @return RunningMode
     * @todo implementation
     */
    RunningMode GetRunningMode2D() const;

    /**
     * @brief set maximum num of output of 2d code
     * @param n unsigned int
     */
    void SetNumsMaxOutput2D(unsigned int n);

    /**
     * @brief get maximum num of output of 2d code
     * @return unsigned int
     */
    unsigned int GetNumsMaxOutput2D() const;

    /**
     * @brief set qr distortion enable signal
     *
     * @param value  bool
     */
    void SetQrDistortionEnable(bool value);

    /**
     * @brief get qr distortion enable signal
     *
     * @return bool
     */
    bool GetQrDistortionEnable() const;

    /**
     * @brief Set the Qr Model Type object
     *
     * @param model_type
     */
    void SetQrModelType(QRModelType model_type);

    /**
     * @brief Get the Qr Model Type object
     *
     * @return QRModelType
     */
    QRModelType GetQrModelType() const;

    /**
     * @brief Set the Dm ECC Type object
     *
     * @param ecc_type
     */
    void SetDmECCType(DMECCType ecc_type);

    /**
     * @brief Get the Dm ECC Type object
     *
     * @return DMECCType
     */
    DMECCType GetDmECCType() const;

    /**
     * @brief set edge type of dm code
     * @param edge_type DMEdgeType::kxx
     * @todo implementation
     */
    void SetDmEdgeType(DMEdgeType edge_type);

    /**
     * @brief get edge type of dm code
     * @return DMEdgeType
     * @todo implementation
     */
    DMEdgeType GetDmEdgeType() const;

    /**
     * @brief set shape type of dm code
     * @param shape_type DMShapeType::kxx
     */
    void SetDmShapeType(DMShapeType shape_type);

    /**
     * @brief get shape type of dm code
     * @return DMShapeType
     */
    DMShapeType GetDmShapeType() const;

    /**
     * @brief set dm distortion enable signal
     *
     * @param value  bool
     */
    void SetDmDistortionEnable(bool value);

    /**
     * @brief get dm distortion enable signal
     *
     * @return bool
     */
    bool GetDmDistortionEnable() const;

    /**
     * @brief set image filter operators
     * @param img_filters {ImgProcOp, ...}
     */
    void SetImageFilterOps(const std::vector<ImgProcOp> &img_filters);

    /**
     * @brief get image filter operators
     * @return std::vector<ImgProcOp>
     */
    std::vector<ImgProcOp> &GetImageFilterOps() const;

    /**
     * @brief set polarity type in 1d
     * @param value
     */
    void SetPolarity1D(Polarity type);

    /**
     * @brief get polarity type in 1d
     * @return Polarity
     */
    Polarity GetPolarity1D(void) const;

    /**
     * @brief set maximum number of output decoded messages in 1d
     * @param value
     */
    void SetNumsMaxOutput1D(unsigned int value);

    /**
     * @brief get maximum number of output decoded messages in 1d
     * @return unsigned int
     */
    unsigned int GetNumsMaxOutput1D(void) const;

    /**
     * @brief Set running mode
     * @param bool
     */
    void SetFullScan1D(bool enable_full_scan);

    /**
     * @brief Get running mode
     * @return bool
     */
    bool GetFullScan1D(void) const;

    /**
     * @brief set code39 check digit enable signal
     * @param value
     */
    void SetCheckDigitEnableCode39(bool value);

    /**
     * @brief get code39 check digit enable signal
     * @return bool
     */
    bool GetCheckDigitEnableCode39(void) const;

    /**
     * @brief set itf check digit enable signal
     * @param value
     */
    void SetCheckDigitEnableITF(bool value);

    /**
     * @brief get itf check digit enable signal
     * @return bool
     */
    bool GetCheckDigitEnableITF(void) const;

    /**
     * @brief temp interface for log
     * @param value
     */
    void SetLogEnable(bool value);

    /**
     * @brief set dl detect nums of instance
     * @param value
     */
    void SetDLNumsInstance(unsigned int value);

    /**
     * @brief get dl detect nums of instance
     * @return unsigned int
     */
    unsigned int GetDLNumsInstance(void) const;

    /**
     * @brief set dl detect polys output score threshold
     * @param value
     */
    void SetDLScoreThresh(float value);

    /**
     * @brief get dl detect polys output score threshold
     * @return unsigned int
     */
    float GetDLScoreThresh(void) const;

    /**
     * @brief set rois
     * @param rois std::vector<Roi>
     */
    void SetRois(const std::vector<Roi> &rois);

    /**
     * @brief get rois
     * @return std::vector<Roi>
     */
    std::vector<Roi> &GetRois(void) const;

    /**
     * @brief clear rois
     */
    void ClearRois();

    /**
     * @brief set 2d quality calculation enable
     * @param value bool
     */
    void Set2DQualityEnable(bool value);

    /**
     * @brief get 2d quality calculation enable
     * @return bool
     */
    bool Get2DQualityEnable(void) const;

    /**
     * @brief set 1d quality calculation enable
     * @param value bool
     */
    void Set1DQualityEnable(bool value);

    /**
     * @brief get 1d quality calculation enable
     * @return bool
     */
    bool Get1DQualityEnable(void) const;

    /**
     * @brief set dm pyramid level
     * @param value unsigned int > 1
     */
    void SetDMPyramidLevel(unsigned int value);

    /**
     * @brief get dm pyramid level
     * @return unsigned int
     */
    unsigned int GetDMPyramidLevel(void) const;

    /**
     * @brief set version for DM code
     * @param std::array<int, 2>
     */
    void SetDMVersion(const std::array<int, 2> &version);

    /**
     * @brief get version for DM code
     * @return version std::array<int, 2>
     */
    std::array<int, 2> GetDMVersion(void) const;

    /**
     * @brief set version for QR code
     * @param int
     */
    void SetQRVersion(int version);

    /**
     * @brief get version for QR code
     * @return version int
     */
    int GetQRVersion(void);

    /**
     * @brief set min ppm for 2D barcode
     * @param float
     */
    void SetMinPPM2D(float value);

    /**
     * @brief get min ppm for 2D barcode
     * @return value float
     */
    float GetMinPPM2D(void) const;

    /**
     * @brief set min ppm for 1D barcode
     * @param float
     */
    void SetMinPPM1D(float value);

    /**
     * @brief get min ppm for 1D barcode
     * @return value float
     */
    float GetMinPPM1D(void) const;

    /**
     * @brief set pre-locate scale for 1D barcode
     * @param float
     */
    void SetPrelocateScale1D(const float scale);

    /**
     * @brief get pre-locate scale for 1D barcode
     * @return value float
     */
    float GetPrelocateScale1D(void) const;

    /**
     * @brief set config signature:
     * @param value string,for example nxp100w,nxp200w,android, more information
     * in config_dl.json
     */
    void SetConfigSignature(std::string value);

    /**
     * @brief get config signature
     * @return string
     */
    std::string GetConfigSignature(void) const;

    /**
     * @brief set quality verify iso standard
     * @param ISOStandard
     */
    void SetISOStandard(ISOStandard standard);

    /**
     * @brief get quality verify iso standard
     * @param ISOStandard
     */
    ISOStandard GetISOStandard(void) const;

    /**
     * @brief set quota enable and threshold in 1d
     * @param BarcodeParams1DQuota
     */
    void SetQuota1D(BarcodeParams1DQuota quota_1d);

    /**
     * @brief set quota enable and threshold in 2d
     * @param BarcodeParams1DQuota
     */
    void SetQuota2D(BarcodeParams2DQuota quota_1d);

    // /**
    //  * @brief set quota enable and threshold in qr
    //  * @param BarcodeParamsQRQuota
    //  */
    // void SetQuotaQR(BarcodeParamsQRQuota quota_qr);

    // /**
    //  * @brief set quota enable and threshold in dm
    //  * @param BarcodeParamsDMQuota
    //  */
    // void SetQuotaDM(BarcodeParamsDMQuota quota_dm);

    /**
     * @brief set decode times for pharmacode
     * @param int
     */
    void SetPharmaMinDecodeTimes1D(const int value);

    /**
     * @brief get decode times for pharmacode
     * @return value int
     */
    int GetPharmaMinDecodeTimes1D(void) const;

    /**
     * @brief set bar count for pharmacode
     * @param int
     */
    void SetPharmaBarCount1D(const int value);

    /**
     * @brief get bar count for pharmacode
     * @return value int
     */
    int GetPharmaBarCount1D(void) const;

    /**
     * @brief set result filter string Regular Expression
     * @param string
     */
    void SetScriptFilterString(const std::string &exp);

    /**
     * @brief get result filter string Regular Expression
     * @return string
     */
    std::string &GetScriptFilterString(void) const;

#ifdef WITH_NOVAIC
    /**
     * @brief close vs600 npu network
     * @return bool
     */
    bool CloseNetWork();
#endif

    /**
     * @brief set Handheld Mode and Center point
     * @param handheld_mode HandheldModeType,for example kUnSet, kFull, kCenter
     * @param handheld_center cv::Point2f, Handheld Center coordinates of Aiming
     * light
     * @return void
     */
    void SetHandheldMode(HandheldModeType handheld_mode,
                         const cv::Point2f &handheld_center);

    /**
     * @brief get Handheld Mode
     * @return HandheldModeType
     */
    HandheldModeType &GetHandheldMode(void) const;

    /**
     * @brief get Handheld Center coordinates of Aiming light
     * @return cv::Point2f, center coordinates
     */
    cv::Point2f &GetHandheldCenter(void) const;

    /**
     * @brief set center Roi box WH of Handheld kCenterROI Mode
     * @param handheld_centerWH In HandheldMode KCenterROI, Width and Height of
     * CenterROI Box
     * @return void
     */
    void SetHandheldCenterWH(const std::array<int, 2> &handheld_centerWH);

    /**
     * @brief get W and H of Handheld Center ROI
     * @return std::array<int, 2>, center roi box WH
     */
    std::array<int, 2> &GetHandheldCenterWH(void) const;

    /**
     * @brief image gamma val
     * @param gamma gamma, must be in range of [0.1, 4.0] with the step of 0.1
     */
    void SetGammaVal(double gamma);

private:
    /**
     * @brief impl_ ptr
     */
    std::shared_ptr<BarcodeImpl> impl_;
};

/**
 * @brief image gamma correction
 * @param img
 * @param dst_img
 * @param gamma gamma, must be in range of [0.1, 4.0] with the step of 0.1
 */
void GammaCorrection(const cv::Mat &img, cv::Mat &dst_img, const double gamma);

} // namespace barcode

} // namespace smartmore

#endif // __SMARTMORE_BARCODE_H__
