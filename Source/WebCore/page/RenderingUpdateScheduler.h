/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "DisplayRefreshMonitorClient.h"
#include <wtf/Seconds.h>

namespace WebCore {

class Page;
class Timer;

class RenderingUpdateScheduler
#if USE(REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR)
    : public DisplayRefreshMonitorClient
#endif
{
    WTF_MAKE_FAST_ALLOCATED;
public:
    static std::unique_ptr<RenderingUpdateScheduler> create(Page& page)
    {
        return makeUnique<RenderingUpdateScheduler>(page);
    }

    RenderingUpdateScheduler(Page&);
    
    void adjustRenderingUpdateFrequency();
    void scheduleTimedRenderingUpdate();
    void scheduleImmediateRenderingUpdate();
    void scheduleRenderingUpdate();

#if USE(REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR)
    void windowScreenDidChange(PlatformDisplayID);
#endif

private:
#if USE(REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR)
#if PLATFORM(IOS_FAMILY)
    void adjustFramesPerSecond();
#endif
    RefPtr<DisplayRefreshMonitor> createDisplayRefreshMonitor(PlatformDisplayID) const final;
    void displayRefreshFired() final;
#else
    void displayRefreshFired();
#endif

    bool isScheduled() const;
    void startTimer(Seconds);
    void clearScheduled();

    Page& m_page;
#if USE(REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR) && PLATFORM(IOS_FAMILY)
    bool m_isMonitorCreated;
#endif
    bool m_scheduled { false };
    std::unique_ptr<Timer> m_refreshTimer;
};

}
