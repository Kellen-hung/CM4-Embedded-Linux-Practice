#pragma once

#include <opencv2/core.hpp>

#include <memory>
#include <string>

enum class Decoder {
    Hardware,
    Software,
};

class RtspFrame {
    public:
        // constructor
        RtspFrame();
        // destructor
        ~RtspFrame();
        // move constructor
        RtspFrame(RtspFrame&&) noexcept;
        // move assignment
        RtspFrame& operator=(RtspFrame&&) noexcept;
        // copy constructor
        RtspFrame(const RtspFrame&) = delete;
        // copy assignment
        RtspFrame& operator=(const RtspFrame&) = delete;

        const cv::Mat& image() const;
        explicit operator bool() const;

    private:
        // PImpl
        struct Impl;
        std::unique_ptr<Impl> impl_;

        explicit RtspFrame(std::unique_ptr<Impl> impl);
        friend class RtspCapture;
};

class RtspCapture {
    public:
        RtspCapture(const std::string& url, Decoder decoder = Decoder::Hardware);
        ~RtspCapture();
        RtspCapture(const RtspCapture&) = delete;
        RtspCapture& operator=(const RtspCapture&) = delete;

        RtspFrame pullFrame(unsigned int timeout_ms = 200);
        bool checkError(std::string& message);

    private:
        // PImpl
        struct Impl;
        std::unique_ptr<Impl> impl_;
};
