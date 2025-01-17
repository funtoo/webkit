/*
 * Copyright (C) 2017-2019 Apple Inc. All rights reserved.
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
#include "WebCoreJSClientData.h"

#include "DOMGCOutputConstraint.h"
#include "JSDOMBinding.h"
#include "JSDOMBuiltinConstructorBase.h"
#include "JSDOMWindow.h"
#include "JSDOMWindowProperties.h"
#include "JSDedicatedWorkerGlobalScope.h"
#include "JSPaintWorkletGlobalScope.h"
#include "JSRemoteDOMWindow.h"
#include "JSServiceWorkerGlobalScope.h"
#include "JSWindowProxy.h"
#include "JSWorkerGlobalScope.h"
#include "JSWorkletGlobalScope.h"
#include <JavaScriptCore/FastMallocAlignedMemoryAllocator.h>
#include <JavaScriptCore/HeapInlines.h>
#include <JavaScriptCore/IsoHeapCellType.h>
#include <JavaScriptCore/JSDestructibleObjectHeapCellType.h>
#include <JavaScriptCore/MarkingConstraint.h>
#include <JavaScriptCore/SubspaceInlines.h>
#include <JavaScriptCore/VM.h>
#include "runtime_array.h"
#include "runtime_method.h"
#include "runtime_object.h"
#include <wtf/MainThread.h>

namespace WebCore {
using namespace JSC;

JSVMClientData::JSVMClientData(VM& vm)
    : m_builtinFunctions(vm)
    , m_builtinNames(vm)
    , m_runtimeArrayHeapCellType(JSC::IsoHeapCellType::create<RuntimeArray>())
    , m_runtimeObjectHeapCellType(JSC::IsoHeapCellType::create<JSC::Bindings::RuntimeObject>())
    , m_windowProxyHeapCellType(JSC::IsoHeapCellType::create<JSWindowProxy>())
    , m_heapCellTypeForJSDOMWindow(JSC::IsoHeapCellType::create<JSDOMWindow>())
    , m_heapCellTypeForJSDedicatedWorkerGlobalScope(JSC::IsoHeapCellType::create<JSDedicatedWorkerGlobalScope>())
    , m_heapCellTypeForJSRemoteDOMWindow(JSC::IsoHeapCellType::create<JSRemoteDOMWindow>())
    , m_heapCellTypeForJSWorkerGlobalScope(JSC::IsoHeapCellType::create<JSWorkerGlobalScope>())
#if ENABLE(SERVICE_WORKER)
    , m_heapCellTypeForJSServiceWorkerGlobalScope(JSC::IsoHeapCellType::create<JSServiceWorkerGlobalScope>())
#endif
#if ENABLE(CSS_PAINTING_API)
    , m_heapCellTypeForJSPaintWorkletGlobalScope(JSC::IsoHeapCellType::create<JSPaintWorkletGlobalScope>())
    , m_heapCellTypeForJSWorkletGlobalScope(JSC::IsoHeapCellType::create<JSWorkletGlobalScope>())
#endif
    , m_domBuiltinConstructorSpace ISO_SUBSPACE_INIT(vm.heap, vm.cellHeapCellType.get(), JSDOMBuiltinConstructorBase)
    , m_domConstructorSpace ISO_SUBSPACE_INIT(vm.heap, vm.cellHeapCellType.get(), JSDOMConstructorBase)
    , m_domWindowPropertiesSpace ISO_SUBSPACE_INIT(vm.heap, vm.cellHeapCellType.get(), JSDOMWindowProperties)
    , m_runtimeArraySpace ISO_SUBSPACE_INIT(vm.heap, m_runtimeArrayHeapCellType.get(), RuntimeArray)
    , m_runtimeMethodSpace ISO_SUBSPACE_INIT(vm.heap, vm.cellHeapCellType.get(), RuntimeMethod) // Hash:0xf70c4a85
    , m_runtimeObjectSpace ISO_SUBSPACE_INIT(vm.heap, m_runtimeObjectHeapCellType.get(), JSC::Bindings::RuntimeObject)
    , m_windowProxySpace ISO_SUBSPACE_INIT(vm.heap, m_windowProxyHeapCellType.get(), JSWindowProxy)
    , m_subspaceForJSDOMWindow ISO_SUBSPACE_INIT(vm.heap, m_heapCellTypeForJSDOMWindow.get(), JSDOMWindow)
    , m_subspaceForJSDedicatedWorkerGlobalScope ISO_SUBSPACE_INIT(vm.heap, m_heapCellTypeForJSDedicatedWorkerGlobalScope.get(), JSDedicatedWorkerGlobalScope)
    , m_subspaceForJSRemoteDOMWindow ISO_SUBSPACE_INIT(vm.heap, m_heapCellTypeForJSRemoteDOMWindow.get(), JSRemoteDOMWindow)
    , m_subspaceForJSWorkerGlobalScope ISO_SUBSPACE_INIT(vm.heap, m_heapCellTypeForJSWorkerGlobalScope.get(), JSWorkerGlobalScope)
#if ENABLE(SERVICE_WORKER)
    , m_subspaceForJSServiceWorkerGlobalScope ISO_SUBSPACE_INIT(vm.heap, m_heapCellTypeForJSServiceWorkerGlobalScope.get(), JSServiceWorkerGlobalScope)
#endif
#if ENABLE(CSS_PAINTING_API)
    , m_subspaceForJSPaintWorkletGlobalScope ISO_SUBSPACE_INIT(vm.heap, m_heapCellTypeForJSPaintWorkletGlobalScope.get(), JSPaintWorkletGlobalScope)
    , m_subspaceForJSWorkletGlobalScope ISO_SUBSPACE_INIT(vm.heap, m_heapCellTypeForJSWorkletGlobalScope.get(), JSWorkletGlobalScope)
#endif
    , m_outputConstraintSpace("WebCore Wrapper w/ Output Constraint", vm.heap, vm.destructibleObjectHeapCellType.get(), vm.fastMallocAllocator.get()) // Hash:0x7724c2e4
{
}

JSVMClientData::~JSVMClientData()
{
    ASSERT(m_worldSet.contains(m_normalWorld.get()));
    ASSERT(m_worldSet.size() == 1);
    ASSERT(m_normalWorld->hasOneRef());
    m_normalWorld = nullptr;
    ASSERT(m_worldSet.isEmpty());
}

void JSVMClientData::getAllWorlds(Vector<Ref<DOMWrapperWorld>>& worlds)
{
    ASSERT(worlds.isEmpty());
    
    worlds.reserveInitialCapacity(m_worldSet.size());
    for (auto it = m_worldSet.begin(), end = m_worldSet.end(); it != end; ++it)
        worlds.uncheckedAppend(*(*it));
}

void JSVMClientData::initNormalWorld(VM* vm)
{
    JSVMClientData* clientData = new JSVMClientData(*vm);
    vm->clientData = clientData; // ~VM deletes this pointer.

    vm->heap.addMarkingConstraint(makeUnique<DOMGCOutputConstraint>(*vm, *clientData));

    clientData->m_normalWorld = DOMWrapperWorld::create(*vm, DOMWrapperWorld::Type::Normal);
    vm->m_typedArrayController = adoptRef(new WebCoreTypedArrayController());
}

} // namespace WebCore

