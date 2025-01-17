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

#include "config.h"
#include "FeaturePolicy.h"

#include "Document.h"
#include "HTMLIFrameElement.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "SecurityOrigin.h"

namespace WebCore {

using namespace HTMLNames;

bool isFeaturePolicyAllowedByDocumentAndAllOwners(FeaturePolicy::Type type, const Document& document)
{
    auto& topDocument = document.topDocument();
    auto ancestorDocument = &document;
    while (ancestorDocument != &topDocument) {
        if (!ancestorDocument)
            return false;

        auto ownerElement = ancestorDocument->ownerElement();
        if (is<HTMLIFrameElement>(ownerElement)) {
            auto featurePolicy = downcast<HTMLIFrameElement>(ownerElement)->featurePolicy();
            if (!featurePolicy.allows(type, ancestorDocument->securityOrigin().data()))
                return false;
        }

        ancestorDocument = ancestorDocument->parentDocument();
    }

    return true;
}

static bool isAllowedByFeaturePolicy(const FeaturePolicy::AllowRule& rule, const SecurityOriginData& origin)
{
    switch (rule.type) {
    case FeaturePolicy::AllowRule::Type::None:
        return false;
    case FeaturePolicy::AllowRule::Type::All:
        return true;
    case FeaturePolicy::AllowRule::Type::List:
        return rule.allowedList.contains(origin);
    }
    ASSERT_NOT_REACHED();
    return false;
}

static inline void processOriginItem(Document& document, FeaturePolicy::AllowRule& rule, StringView item)
{
    if (rule.type == FeaturePolicy::AllowRule::Type::None)
        return;

    item = item.stripLeadingAndTrailingMatchedCharacters(isHTMLSpace<UChar>);
    // FIXME: Support 'src'.
    if (item == "'src'")
        return;

    if (item == "*") {
        rule.type = FeaturePolicy::AllowRule::Type::All;
        return;
    }

    if (item == "'self'") {
        rule.allowedList.add(document.securityOrigin().data());
        return;
    }
    if (item == "'none'") {
        rule.type = FeaturePolicy::AllowRule::Type::None;
        return;
    }
    URL url { { }, item.toString() };
    if (url.isValid())
        rule.allowedList.add(SecurityOriginData::fromURL(url));
}

static inline void updateList(Document& document, FeaturePolicy::AllowRule& rule, StringView value)
{
    // We keep the empty string value equivalent to '*' for existing websites.
    if (value.isEmpty()) {
        rule.type = FeaturePolicy::AllowRule::Type::All;
        return;
    }

    while (!value.isEmpty()) {
        auto position = value.find(isHTMLSpace<UChar>);
        if (position == notFound) {
            processOriginItem(document, rule, value);
            return;
        }

        processOriginItem(document, rule, value.substring(0, position));
        value = value.substring(position + 1).stripLeadingAndTrailingMatchedCharacters(isHTMLSpace<UChar>);
    }
}

FeaturePolicy FeaturePolicy::parse(Document& document, const HTMLIFrameElement& iframe, StringView allowAttributeValue)
{
    FeaturePolicy policy;
    bool isCameraInitialized = false;
    bool isMicrophoneInitialized = false;
    bool isDisplayCaptureInitialized = false;
    bool isSyncXHRInitialized = false;
    bool isFullscreenInitialized = false;
    for (auto allowItem : allowAttributeValue.split(';')) {
        auto item = allowItem.stripLeadingAndTrailingMatchedCharacters(isHTMLSpace<UChar>);
        if (item.startsWith("camera")) {
            isCameraInitialized = true;
            updateList(document, policy.m_cameraRule, item.substring(7));
            continue;
        }
        if (item.startsWith("microphone")) {
            isMicrophoneInitialized = true;
            updateList(document, policy.m_microphoneRule, item.substring(11));
            continue;
        }
        if (item.startsWith("display-capture")) {
            isDisplayCaptureInitialized = true;
            updateList(document, policy.m_displayCaptureRule, item.substring(16));
            continue;
        }
        if (item.startsWith("sync-xhr")) {
            isSyncXHRInitialized = true;
            updateList(document, policy.m_syncXHRRule, item.substring(8));
            continue;
        }
        if (item.startsWith("fullscreen")) {
            isFullscreenInitialized = true;
            updateList(document, policy.m_fullscreenRule, item.substring(11));
            continue;
        }
    }

    // By default, camera, microphone, display-capture, and fullscreen policy is 'self'
    if (!isCameraInitialized)
        policy.m_cameraRule.allowedList.add(document.securityOrigin().data());
    if (!isMicrophoneInitialized)
        policy.m_microphoneRule.allowedList.add(document.securityOrigin().data());
    if (!isDisplayCaptureInitialized)
        policy.m_displayCaptureRule.allowedList.add(document.securityOrigin().data());

    // https://w3c.github.io/webappsec-feature-policy/#process-feature-policy-attributes
    // 9.5 Process Feature Policy Attributes
    // 3.1 If element’s allowfullscreen attribute is specified, and container policy does
    //     not contain an allowlist for fullscreen,
    if (!isFullscreenInitialized) {
        if (iframe.hasAttribute(allowfullscreenAttr) || iframe.hasAttribute(webkitallowfullscreenAttr)) {
            // 3.1.1 Construct a new declaration for fullscreen, whose allowlist is the special value *.
            policy.m_fullscreenRule.type = FeaturePolicy::AllowRule::Type::All;
        } else {
            // https://fullscreen.spec.whatwg.org/#feature-policy-integration
            // The default allowlist is 'self'.
            policy.m_fullscreenRule.allowedList.add(document.securityOrigin().data());
        }
    }

    if (!isSyncXHRInitialized)
        policy.m_syncXHRRule.type = AllowRule::Type::All;

    return policy;
}

bool FeaturePolicy::allows(Type type, const SecurityOriginData& origin) const
{
    switch (type) {
    case Type::Camera:
        return isAllowedByFeaturePolicy(m_cameraRule, origin);
    case Type::Microphone:
        return isAllowedByFeaturePolicy(m_microphoneRule, origin);
    case Type::DisplayCapture:
        return isAllowedByFeaturePolicy(m_displayCaptureRule, origin);
    case Type::SyncXHR:
        return isAllowedByFeaturePolicy(m_syncXHRRule, origin);
    case Type::Fullscreen:
        return isAllowedByFeaturePolicy(m_fullscreenRule, origin);
    }
    ASSERT_NOT_REACHED();
    return false;
}

}
