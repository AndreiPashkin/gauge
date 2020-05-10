#ifndef CHRONO_UTILS_HPP
#define CHRONO_UTILS_HPP
#include "cmath"

namespace gauge {
    namespace detail {
        /**
         * Provides facilities for conversion of time-points between clocks.
         *
         * Primary use-case here is conversion of time-points taken from
         * the monotonic clock to time-points expressed in terms of "normal"
         * clock.
         *
         * Usage:
         * @code{.cpp}
         * auto const t1 = chrono::steady_clock::now();
         * auto const t2 = TimePointConversionUtil<
         *     std::chrono::steady_clock,
         *     std::chrono::system_clock
         * >::convert_time_point(t1);
         * @endcode
         *
         * Based on snippets from here:
         * https://stackoverflow.com/a/35293183/1818608
         */
        template
            <
                typename SrcClockType,
                typename DstClockType,
                typename SrcDurationType = typename SrcClockType::duration,
                typename SrcTimePointType = typename SrcClockType::time_point,
                typename DstTimePointType = typename DstClockType::time_point
            >
        class TimePointConversionUtil {
        public:
            using BaseMeasurements = std::tuple<SrcTimePointType, DstTimePointType>;

            /**
             * Convert the time-point using already taken base-measurements.
             *
             * @param time_point Time-point to convert.
             * @param base_measurements Base measurements to use.
             * @return Time-point converted to the destination-type clock.
             */
            static DstTimePointType
            convert_time_point(
                const SrcTimePointType &time_point,
                const BaseMeasurements &base_measurements
            ) {
                return (
                    std::get<1>(base_measurements) +
                    (time_point - std::get<0>(base_measurements))
                );
            }

            /**
             * Take base-measurements and convert the time-point using them.
             *
             * @param time_point Time-point to convert.
             * @param tolerance Same as in get_base_measurements().
             * @param limit Same as in get_base_measurements().
             * @return Time-point converted to the destination-type clock.
             */
            static DstTimePointType
            convert_time_point(
                const SrcTimePointType &time_point,
                const SrcDurationType tolerance =
                   std::chrono::nanoseconds{100},
                const int limit = 10
            ) {
                auto const measurements = get_base_measurements(
                    tolerance,
                    limit
                );
                return convert_time_point(time_point, measurements);
            };

            /**
             * Try to get times from both clocks with as small gap as possible.
             *
             * @param tolerance Worst possible difference between measurements.
             * @param limit Number of allowed attempts take close measurements.
             * @return Tuple of measurements of both clocks.
             */
            static BaseMeasurements
            get_base_measurements(
                const SrcDurationType tolerance =
                    std::chrono::nanoseconds{100},
                const int limit = 10
            ) {
                auto iter_count = 0;
                auto src_now = SrcTimePointType{};
                auto dst_now = DstTimePointType{};
                auto epsilon = max_duration<SrcDurationType>();
                do {
                    const auto src_before = SrcClockType::now();
                    const auto dst_between = DstClockType::now();
                    const auto src_after = SrcClockType::now();
                    const auto src_diff = src_after - src_before;
                    const auto delta = SrcDurationType{
                        std::abs(src_diff.count())
                    };
                    if (delta < epsilon) {
                        src_now = src_before + src_diff / 2;
                        dst_now = dst_between;
                        epsilon = delta;
                    }
                    if (++iter_count >= limit) {
                        break;
                    }
                } while (epsilon > tolerance);

                return {src_now, dst_now};
            };
        private:
            template<
                typename DurationType,
                typename ReprType = typename DurationType::rep
            >
            static constexpr DurationType
            max_duration() noexcept {
                return DurationType{std::numeric_limits<ReprType>::max()};
            }
        };
    }
}

#endif //CHRONO_UTILS_HPP
