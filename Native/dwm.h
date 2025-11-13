#ifndef DWM_H
#define DWM_H

#include <algorithm>
#include <cassert>
#include <cmath>
#include <vector>

/// Rectilinear Digital Waveguide Mesh implementation,
/// based on the K-DWM implementation described in
/// Kelloniemi, Antti. "Frequency-dependent boundary condition for the 3-D
/// digital waveguide mesh." Proc. Int. Conf. Digital Audio Effects (DAFx’06).
/// 2006.
namespace dwm {

    /// K-DWM boundary filters
    namespace filters {

        /// Struct shared by non-parametric filters
        struct no_parameters {};

        /// K-DWM boundary filter interface
        template<typename parameters>
        class filter {
        public:
            virtual ~filter() = default;

            /// Performs filtering on an incoming sample
            /// @return the new output sample resulting from the filter's computation
            /// @param input the incoming sample
            virtual float process(const parameters &, float input) = 0;
        };

        /// K-DWM boundary Anechoic filter
        class anechoic final : public filter<no_parameters> {
            float prev_input = 0.0f;

        public:
            /// @copydoc filter::process
            float process(const no_parameters &, const float input) override {
                const float result = prev_input;
                prev_input = input;
                return result;
            }
        };

        /// Admittance + Single pole low pass filter parameters
        struct admittance_lowpass_parameters final {
            float admittance;
            float cutoff;

            /// Constructor to safely build parameters which do not amplify the signal
            /// @param normalized_admittance must be in the [0,1] range
            /// @param normalized_cutoff must be in the [0,1] range
            explicit admittance_lowpass_parameters(const float normalized_admittance, const float normalized_cutoff) :
                admittance(normalized_admittance - normalized_admittance * (1 - normalized_cutoff) * 0.5f),
                cutoff((1 - normalized_cutoff) * 0.25f * normalized_admittance) {}
        };

        /// Admittance + Single pole low pass filter
        class admittance_lowpass final : public filter<admittance_lowpass_parameters> {
            float prev_input_1 = 0.0f;
            float prev_input_2 = 0.0f;

        public:
            /// @copydoc filter::process
            float process(const admittance_lowpass_parameters &params, const float input) override {
                const float result = params.cutoff * (input + prev_input_2) + (1.0f + params.admittance) * prev_input_1;

                prev_input_2 = prev_input_1;
                prev_input_1 = input;

                return result;
            }
        };

    } // namespace filters

    /// 1-dimensional K-DWM boundary parameterized by a filter
    template<typename filter, typename filter_parameters>
        requires(std::is_base_of_v<filters::filter<filter_parameters>, filter>)
    class mesh_boundary final {
    public:
        /// Updates the boundary state and filters an incoming sample
        /// @return the filtered input K value
        /// @param filter_params the filter's parameters
        /// @param incoming_value the incoming K value
        [[nodiscard]] float update(const filter_parameters &filter_params, const float incoming_value) {
            const float p_plus = incoming_value - p_minus_1;
            const float p_out = f.process(filter_params, p_plus);

            p_minus_1 = p_out - p_plus_1;
            p_plus_1 = p_plus;

            return p_out;
        }

    private:
        float p_plus_1 = 0.0f, p_minus_1 = 0.0f;
        filter f = filter();
    };

    /// 3-dimensional Rectilinear K-DWM implementation
    /// @param width width in meters of the mesh
    /// @param height height in meters of the mesh
    /// @param depth depth in meters of the mesh
    /// @param sample_rate sample rate of the input signal
    /// @param xp_filter filter for the x+ boundary
    /// @param xp_filter_params x+ boundary filter parameters type
    /// @param xn_filter filter implementation for the x- boundary
    /// @param xn_filter_params x- boundary filter parameters type
    /// @param yp_filter filter implementation for the y+ boundary
    /// @param yp_filter_params y+ boundary filter parameters type
    /// @param yn_filter filter implementation for the y- boundary
    /// @param yn_filter_params y- boundary filter parameters type
    /// @param zp_filter filter implementation for the z+ boundary
    /// @param zp_filter_params z+ boundary filter parameters type
    /// @param zn_filter filter implementation for the z- boundary
    /// @param zn_filter_params z- boundary filter parameters type
    template<const float width, const float height, const float depth, const int sample_rate, //
             typename xp_filter, typename xp_filter_params, typename xn_filter, typename xn_filter_params,
             typename yp_filter, typename yp_filter_params, typename yn_filter, typename yn_filter_params,
             typename zp_filter, typename zp_filter_params, typename zn_filter, typename zn_filter_params>
        requires(std::is_base_of_v<filters::filter<xp_filter_params>, xp_filter>,
                 std::is_base_of_v<filters::filter<xn_filter_params>, xn_filter>,
                 std::is_base_of_v<filters::filter<yp_filter_params>, yp_filter>,
                 std::is_base_of_v<filters::filter<yn_filter_params>, yn_filter>,
                 std::is_base_of_v<filters::filter<zp_filter_params>, zp_filter>,
                 std::is_base_of_v<filters::filter<zn_filter_params>, zn_filter>)
    class mesh_3d final {
        // * World coordinates <x, y, z> refer to the continuous coordinate system
        // external to the mesh, which is used
        //   in public facing methods
        // * Junction coordinates <x, y, z> refer to the discrete coordinate system
        // inside the mesh, which is used
        //   internally to the implementation
        // * Linearized coordinates <i> refer to the "unrolled" coordinates used to
        // store the junctions' data
        //   (indices are linearized in x->y->z order, like
        //   [<0,0,0>, <1,0,0>, ... , <s_x,0,0>, <0,1,0>, ..., <s_x,s_y,0>, <0,0,1>,
        //   ..., <s_x,s_y,s_z>])

        // Junctions density, dependent on the mesh's required sample rate
        static constexpr float density = static_cast<float>(sample_rate) / (std::sqrt(3.0f) * 343.0f);
        // Junctions dimensionality, dependent on the mesh's density and "world size"
        static constexpr int size_x = std::max(1, static_cast<int>(std::ceil(width * density)));
        static constexpr int size_y = std::max(1, static_cast<int>(std::ceil(height * density)));
        static constexpr int size_z = std::max(1, static_cast<int>(std::ceil(depth * density)));

        float *p; // Linearized storage of "z timestep" K values for each junction
        float *p_aux; // Linearized storage of "z-1 timestep" K value for each junction

        // Each buffer stores a specific side's boundary

        // x+, size of size_y * size_z, stored in y-major layout
        mesh_boundary<xp_filter, xp_filter_params> *b_xp;
        // x-, size of size_y * size_z, stored in y-major layout
        mesh_boundary<xn_filter, xn_filter_params> *b_xn;
        // y+, size of size_x * size_z, stored in x-major layout
        mesh_boundary<yp_filter, yp_filter_params> *b_yp;
        // y-, size of size_x * size_z, stored in x-major layout
        mesh_boundary<yn_filter, yn_filter_params> *b_yn;
        // z+, size of size_x * size_y, stored in x-major layout
        mesh_boundary<zp_filter, yp_filter_params> *b_zp;
        // z-, size of size_x * size_y, stored in x-major layout
        mesh_boundary<zn_filter, yn_filter_params> *b_zn;

        // Converts from junction coordinates to a linearized coordinates
        [[nodiscard]] static int junction_to_linearized(const int x, const int y, const int z) {
            return (z * size_y + y) * size_x + x;
        }

        // Compute interpolation parameters for a world coordinate
        static void compute_interpolation_parameters(const float x, const float y, const float z, float &px, float &py,
                                              float &pz, int &i000, int &i100, int &i010, int &i110, int &i001,
                                              int &i101, int &i011, int &i111) {
            // World coordinates scaled to match junction coordinates' scale
            const float xs = std::clamp(x, 0.0f, width) * density;
            const float ys = std::clamp(y, 0.0f, height) * density;
            const float zs = std::clamp(z, 0.0f, depth) * density;

            // Get the next and previous junction coordinates for each dimension
            const int x_0 = std::clamp(static_cast<int>(std::floor(xs)), 0, size_x - 1);
            const int y_0 = std::clamp(static_cast<int>(std::floor(ys)), 0, size_y - 1);
            const int z_0 = std::clamp(static_cast<int>(std::floor(zs)), 0, size_z - 1);
            const int x_1 = std::clamp(static_cast<int>(std::ceil(xs)), 0, size_x - 1);
            const int y_1 = std::clamp(static_cast<int>(std::ceil(ys)), 0, size_y - 1);
            const int z_1 = std::clamp(static_cast<int>(std::ceil(zs)), 0, size_z - 1);

            // Return the linearized junction sampling indices
            i000 = junction_to_linearized(x_0, y_0, z_0);
            i100 = junction_to_linearized(x_1, y_0, z_0);
            i010 = junction_to_linearized(x_0, y_1, z_0);
            i110 = junction_to_linearized(x_1, y_1, z_0);
            i001 = junction_to_linearized(x_0, y_0, z_1);
            i101 = junction_to_linearized(x_1, y_0, z_1);
            i011 = junction_to_linearized(x_0, y_1, z_1);
            i111 = junction_to_linearized(x_1, y_1, z_1);

            float _; // Return the horizontal and vertical interpolation percentages
            px = modff(xs, &_);
            py = modff(ys, &_);
            pz = modff(zs, &_);
        }

    public:
        /// Builds a new instance\n
        /// The mesh's valid coordinates range from (0,0) to (width, height, depth)
        explicit mesh_3d() {
            static_assert(width > 0 && "width must be greater than zero");
            static_assert(height > 0 && "height must be be greater than zero");
            static_assert(depth > 0 && "depth must be greater than zero");
            static_assert(sample_rate > 0 && "sample rate must be greater than zero");

            // Allocate all buffers
            p = new float[size_x * size_y * size_z];
            p_aux = new float[size_x * size_y * size_z];
            b_xp = new mesh_boundary<xp_filter, xp_filter_params>[size_y * size_z];
            b_xn = new mesh_boundary<xn_filter, xn_filter_params>[size_y * size_z];
            b_yp = new mesh_boundary<yp_filter, yp_filter_params>[size_x * size_z];
            b_yn = new mesh_boundary<yn_filter, yn_filter_params>[size_x * size_z];
            b_zp = new mesh_boundary<zp_filter, zp_filter_params>[size_x * size_y];
            b_zn = new mesh_boundary<zn_filter, zn_filter_params>[size_x * size_y];
            reset();
        }

        /// Resets the mesh to the initial state
        void reset() {
            std::fill_n(p, size_x * size_y * size_z, 0.0f);
            std::fill_n(p_aux, size_x * size_y * size_z, 0.0f);
            std::fill_n(b_xp, size_y * size_z, mesh_boundary<xp_filter, xp_filter_params>());
            std::fill_n(b_xn, size_y * size_z, mesh_boundary<xn_filter, xn_filter_params>());
            std::fill_n(b_yp, size_x * size_z, mesh_boundary<yp_filter, yp_filter_params>());
            std::fill_n(b_yn, size_x * size_z, mesh_boundary<yn_filter, yn_filter_params>());
            std::fill_n(b_zp, size_x * size_y, mesh_boundary<zp_filter, zp_filter_params>());
            std::fill_n(b_zn, size_x * size_y, mesh_boundary<zn_filter, zn_filter_params>());
        }

        ~mesh_3d() {
            delete[] p;
            delete[] p_aux;
            delete[] b_xp;
            delete[] b_xn;
            delete[] b_yp;
            delete[] b_yn;
            delete[] b_zp;
            delete[] b_zn;
        }
        // No copy constructor
        mesh_3d(const mesh_3d &other) = delete;
        // No copy assignment operator
        mesh_3d &operator=(const mesh_3d &other) = delete;
        // No move constructor
        mesh_3d(mesh_3d &&other) noexcept = delete;
        // No move assignment operator
        mesh_3d &operator=(mesh_3d &&other) noexcept = delete;

        /// Read the value at the specified coordinates inside the mesh
        /// @param x x coordinate
        /// @param y y coordinate
        /// @param z z coordinate
        /// @return value sampled at coordinates (x, y, z)
        /// @attention coordinates outside the ((0, width), (0, height), (0, depth))
        /// range are clamped at the edges
        [[nodiscard]] float read_value(const float x, const float y, const float z) const {
            float px, py, pz;
            int i000, i100, i010, i110, i001, i101, i011, i111;
            compute_interpolation_parameters(x, y, z, px, py, pz, i000, i100, i010, i110, i001, i101, i011, i111);
            return std::lerp(std::lerp(std::lerp(p[i000], p[i100], px), std::lerp(p[i010], p[i110], px), py),
                             std::lerp(std::lerp(p[i001], p[i101], px), std::lerp(p[i011], p[i111], px), py), pz);
        }

        /// Writes a value at the specified coordinates inside the mesh
        /// @param x x coordinate
        /// @param y y coordinate
        /// @param z z coordinate
        /// @param value value written at the coordinates
        /// @attention coordinates outside the ((0, width), (0, height), (0, depth))
        /// range are clamped at the edges
        // ReSharper disable once CppMemberFunctionMayBeConst
        void write_value(const float x, const float y, const float z, const float value) {
            float px, py, pz;
            int i000, i100, i010, i110, i001, i101, i011, i111;
            compute_interpolation_parameters(x, y, z, px, py, pz, i000, i100, i010, i110, i001, i101, i011, i111);
            p[i000] = std::lerp(p[i000], value, (1 - px) * (1 - py) * (1 - pz));
            p[i100] = std::lerp(p[i100], value, px * (1 - py) * (1 - pz));
            p[i010] = std::lerp(p[i010], value, (1 - px) * py * (1 - pz));
            p[i110] = std::lerp(p[i110], value, px * py * (1 - pz));
            p[i001] = std::lerp(p[i001], value, (1 - px) * (1 - py) * pz);
            p[i101] = std::lerp(p[i101], value, px * (1 - py) * pz);
            p[i011] = std::lerp(p[i011], value, (1 - px) * py * pz);
            p[i111] = std::lerp(p[i111], value, px * py * pz);
        }

        /// Updates the mesh's simulation by one sample step
        /// @param xp_params x+ boundary filters parameters
        /// @param xn_params x- boundary filters parameters
        /// @param yp_params y+ boundary filters parameters
        /// @param yn_params y- boundary filters parameters
        /// @param zp_params z+ boundary filters parameters
        /// @param zn_params z- boundary filters parameters
        void update(const xp_filter_params &xp_params, const xn_filter_params &xn_params,
                    const yp_filter_params &yp_params, const yn_filter_params &yn_params,
                    const zp_filter_params &zp_params, const zn_filter_params &zn_params) {
            // At the start of an update, p_aux contains the (z - 1) timestep values
            // During the update loop, each (z - 1) timestep value is read and
            // overwritten with the (z + 1) value

            // The junctions and boundaries are scanned linearly as they are laid out in
            // memory, and the various edge cases are unrolled

            int i = 0, i_xp = 0, i_xn = 0, i_yp = 0, i_yn = 0, i_zp = 0, i_zn = 0;

#define UPDATE(XP, XN, YP, YN, ZP, ZN)                                                                                 \
    p_aux[i] = (XP + XN + YP + YN + ZP + ZN) / 3.0f - p_aux[i];                                                        \
    i++;

#define XP_INTERNAL p[i + 1]
#define XN_INTERNAL p[i - 1]
#define XP_BOUNDARY b_xp[i_xp++].update(xp_params, p[i])
#define XN_BOUNDARY b_xn[i_xn++].update(xn_params, p[i])
#define EDGE_MACRO_X(YP, YN, ZP, ZN)                                                                                   \
    UPDATE(XP_INTERNAL, XN_BOUNDARY, YP, YN, ZP, ZN)                                                                   \
    for (int x = 1; x < size_x - 1; x++) {                                                                             \
        UPDATE(XP_INTERNAL, XN_INTERNAL, YP, YN, ZP, ZN)                                                               \
    }                                                                                                                  \
    UPDATE(XP_BOUNDARY, XN_INTERNAL, YP, YN, ZP, ZN)

#define YP_INTERNAL p[i + size_x]
#define YN_INTERNAL p[i - size_x]
#define YP_BOUNDARY b_yp[i_yp++].update(yp_params, p[i])
#define YN_BOUNDARY b_yn[i_yn++].update(yn_params, p[i])
#define EDGE_MACRO_Y(ZP, ZN)                                                                                           \
    EDGE_MACRO_X(YP_INTERNAL, YN_BOUNDARY, ZP, ZN)                                                                     \
    for (int y = 1; y < size_y - 1; y++) {                                                                             \
        EDGE_MACRO_X(YP_INTERNAL, YN_INTERNAL, ZP, ZN)                                                                 \
    }                                                                                                                  \
    EDGE_MACRO_X(YP_BOUNDARY, YN_INTERNAL, ZP, ZN)

#define ZP_INTERNAL p[i + size_x * size_y]
#define ZN_INTERNAL p[i - size_x * size_y]
#define ZP_BOUNDARY b_zp[i_zp++].update(zp_params, p[i])
#define ZN_BOUNDARY b_zn[i_zn++].update(zn_params, p[i])
            EDGE_MACRO_Y(ZP_INTERNAL, ZN_BOUNDARY)
            for (int z = 1; z < size_z - 1; z++) {
                EDGE_MACRO_Y(ZP_INTERNAL, ZN_INTERNAL)
            }
            EDGE_MACRO_Y(ZP_BOUNDARY, ZN_INTERNAL)

#undef UPDATE
#undef XP_INTERNAL
#undef XN_INTERNAL
#undef XP_BOUNDARY
#undef XN_BOUNDARY
#undef EDGE_MACRO_X
#undef YP_INTERNAL
#undef YN_INTERNAL
#undef YP_BOUNDARY
#undef YN_BOUNDARY
#undef EDGE_MACRO_Y
#undef ZP_INTERNAL
#undef ZN_INTERNAL
#undef ZP_BOUNDARY
#undef ZN_BOUNDARY

            std::swap(p, p_aux); // Current <-> previous buffer swap
        }
    };
} // namespace dwm

#endif
