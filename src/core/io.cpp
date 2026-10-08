#include "core/io.hpp"
#include "utility/logger.hpp"

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <format>
#include <mutex>
#include <span>
#include <string>

#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

namespace io
{
static int BAUD_RATE = B230400;

Port::Port(std::string path) : mutex(std::mutex())
{

        logger::log(std::format("TRY OPEN [{}]", path));

        this->fd = open((char*)path.data(), O_RDWR | O_NOCTTY);

        if (this->fd == -1) {
                const int openError = errno;
                logger::log(std::format("failed to open {}: errno={}", path, openError));
                std::exit(openError);
        }

        logger::log(std::format(" [{}] OPENED", path));
        logger::log(std::format(" [{}] BAUD_RATE: {}", path, BAUD_RATE));

        this->tty = {};

        cfmakeraw(&this->tty);
        cfsetispeed(&this->tty, BAUD_RATE);
        cfsetospeed(&this->tty, BAUD_RATE);
        // 1文字8bit
        tty.c_cflag &= ~CSIZE;
        this->tty.c_cflag |= CS8;
        // 読み込み
        this->tty.c_cflag |= CREAD;
        // パリティビットなし
        this->tty.c_cflag &= ~PARENB;
        // stop bit 1bit
        this->tty.c_cflag &= ~CSTOPB;
        // キャリア検出とかしない
        this->tty.c_cflag &= ~CLOCAL;
        // 最低1文字読み出し
        tty.c_cc[VMIN] = 1;
        // タイムアウトなし
        tty.c_cc[VTIME] = 0;
        tcsetattr(this->fd, TCSANOW, &this->tty);

        // // ブロッキングに戻す
        // int flag = fcntl(this->fd, F_GETFL, 0);
        // fcntl(this->fd, F_SETFL, flag & ~O_NONBLOCK);
}

Port::~Port()
{
        if (this->fd >= 0)
                close(this->fd);
}

std::expected<void, error::Error> Port::readRaw(std::span<uint8_t> bytes)
{
        ssize_t readSize = 0;
        for (size_t offset = 0; offset < bytes.size(); offset += static_cast<size_t>(readSize)) {

                readSize = read(this->fd, bytes.data() + offset, bytes.size() - offset);

                if (readSize > 0)
                        continue;

                if (readSize < 0 && errno == EINTR) {
                        readSize = 0;
                        continue;
                }

                return std::unexpected(error::Error::IO_READ_FAILED);
        }

        return {};
}

std::expected<void, error::Error> Port::writeRaw(std::span<const uint8_t> bytes)
{
        ssize_t writtenSize = 0;
        for (size_t offset = 0; offset < bytes.size(); offset += static_cast<size_t>(writtenSize)) {

                writtenSize = write(this->fd, bytes.data() + offset, bytes.size() - offset);

                if (writtenSize > 0)
                        continue;

                if (writtenSize < 0 && errno == EINTR) {
                        writtenSize = 0;
                        continue;
                }

                return std::unexpected(error::Error::IO_WRITE_FAILED);
        }

        // if (fsync(this->fd) < 0)
        //         return std::unexpected(error::Error::IO_WRITE_FAILED);

        return {};
}

} // namespace io
