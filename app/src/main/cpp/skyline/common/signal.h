// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common.h>

namespace skyline::signal {
    /**
     * @brief An exception object that is designed specifically to hold Linux signals
     * @note This doesn't inherit std::exception as it shouldn't be caught as such
     * @note Refer to the manpage siginfo(3) for information on members
     */
    class SignalException {
      public:
        int signal{};
        void* pc{};
        void *fault{};

        inline std::string what() const {
            if (!fault)
                return fmt::format("Signal: {} (PC: 0x{:X})", strsignal(signal), reinterpret_cast<uintptr_t>(pc));
            else
                return fmt::format("Signal: {} @ 0x{:X} (PC: 0x{:X})", strsignal(signal), reinterpret_cast<uintptr_t>(fault), reinterpret_cast<uintptr_t>(pc));
        }
    };

    /**
     * @brief A signal handler which automatically throws an exception with the corresponding signal metadata in a SignalException
     * @note A termination handler is set in this which prevents any termination from going through as to break out of 'noexcept', do not use std::terminate in a catch clause for this exception
     */
    void ExceptionalSignalHandler(int signal, siginfo *, ucontext *context);

    /**
     * @brief Our delegator for sigaction, we need to do this due to sigchain hooking bionic's sigaction and it intercepting signals before they're passed onto userspace
     * This not only leads to performance degradation but also requires host TLS to be in the TLS register which we cannot ensure for in-guest signals
     */
    void Sigaction(int signal, const struct sigaction *action, struct sigaction *oldAction = nullptr);

    /**
     * @brief If the TLS value of the code running prior to a signal has a custom TLS value, this should be used to restore it
     * @param function A function which is inert if the TLS isn't required to be restored, it should return nullptr if TLS wasn't restored else the old TLS value
     */
    void SetTlsRestorer(void *(*function)());

    using SignalHandler = void (*)(int, struct siginfo *, ucontext *, void **);

    /**
     * @brief A wrapper around Sigaction to make it easy to set a sigaction signal handler for multiple signals and also allow for thread-local signal handlers
     * @param function A sa_action callback with a pointer to the old TLS (If present) as the 4th argument
     * @note If 'nullptr' is written into the 4th argument then the old TLS won't be restored or it'll be set to any non-null value written into it
     */
    void SetSignalHandler(std::initializer_list<int> signals, SignalHandler function);

    inline void SetSignalHandler(std::initializer_list<int> signals, void (*function)(int, struct siginfo *, ucontext *)) {
        SetSignalHandler(signals, reinterpret_cast<SignalHandler>(function));
    }

    /**
     * @brief Our delegator for sigprocmask, required due to libsigchain hooking this
     */
    void Sigprocmask(int how, const sigset_t &set, sigset_t *oldSet = nullptr);

    inline void BlockSignal(std::initializer_list<int> signals) {
        sigset_t set{};
        for (int signal : signals)
            sigaddset(&set, signal);
        Sigprocmask(SIG_BLOCK, set, nullptr);
    }
}
