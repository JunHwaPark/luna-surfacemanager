// Copyright (c) 2013-2018 LG Electronics, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0

#include <QGuiApplication>
#include <qpa/qplatformnativeinterface.h>

#include <QList>
#include <QDebug>

#include <QWaylandCompositor>
#include <QWaylandSurface>
#include <QWaylandSurfaceItem>
#include <QWaylandInputDevice>
#include <QtCompositor/private/qwlinputdevice_p.h>
#include <QtCompositor/private/qwlsurface_p.h>
#include "waylandinputmethodcontext.h"
#include "waylandinputmethod.h"
#include "waylandinputpanel.h"
#include "waylandtextmodel.h"
#include "waylandinputmethodmanager.h"

#ifdef MULTIINPUT_SUPPORT
#include "../weboscorecompositor.h"
#include "../webosinputdevice.h"
#include "../webosseat/webosseat.h"
#endif

const struct input_method_context_interface WaylandInputMethodContext::inputMethodContextImplementation = {
    WaylandInputMethodContext::destroy,
    WaylandInputMethodContext::commitString,
    WaylandInputMethodContext::preEditString,
    WaylandInputMethodContext::preEditStyling,
    WaylandInputMethodContext::preEditCursor,
    WaylandInputMethodContext::deleteSurroundingText,
    WaylandInputMethodContext::cursorPosition,
    WaylandInputMethodContext::modifiersMap,
    WaylandInputMethodContext::keySym,
    WaylandInputMethodContext::grabKeyboard,
    WaylandInputMethodContext::key,
    WaylandInputMethodContext::modifiers
};

WaylandInputMethodContext::WaylandInputMethodContext(WaylandInputMethod* inputMethod, WaylandTextModel* model)
    : m_inputMethod(inputMethod)
    , m_textModel(model)
    , m_resource(0)
    , m_grabResource(0)
    , m_grabbed(false)
    , m_activated(false)
    , m_resourceCount(0)
{
    qDebug() << this;

    connect(model, SIGNAL(activated()), this, SLOT(activateTextModel()));
    connect(model, SIGNAL(deactivated()), this, SLOT(deactivateTextModel()));
    connect(model, SIGNAL(destroyed()), this, SLOT(destroyTextModel()));
    connect(model, SIGNAL(contentTypeChanged(uint32_t, uint32_t)),
            this, SLOT(updateContentType(uint32_t, uint32_t)));
    connect(model, SIGNAL(enterKeyTypeChanged(uint32_t)),
            this, SLOT(updateEnterKeyType(uint32_t)));
    connect(model, SIGNAL(surroundingTextChanged(const QString&, uint32_t, uint32_t)),
            this, SLOT(updateSurroundingText(const QString&, uint32_t, uint32_t)));
    connect(model, SIGNAL(reset(uint32_t)), this, SLOT(resetContext(uint32_t)));
    connect(model, SIGNAL(commit()), this, SLOT(commit()));
    connect(model, SIGNAL(actionInvoked(uint32_t, uint32_t)), this, SLOT(invokeAction(uint32_t, uint32_t)));
    connect(model, SIGNAL(maxTextLengthChanged(uint32_t)), this, SLOT(maxTextLengthTextModel(uint32_t)));
    connect(model, SIGNAL(platformDataChanged(const QString&)), this, SLOT(platformDataModel(const QString&)));

    connect(m_inputMethod->inputPanel(), &WaylandInputPanel::inputPanelStateChanged,
            this, &WaylandInputMethodContext::updatePanelState);
    connect(m_inputMethod->inputPanel(), &WaylandInputPanel::inputPanelSizeChanged,
            this, &WaylandInputMethodContext::updatePanelSize);
}

void WaylandInputMethodContext::destroyInputMethodContext(struct wl_resource* resource)
{
    WaylandInputMethodContext* that = static_cast<WaylandInputMethodContext*>(resource->data);
    WaylandTextModel* model = that->m_textModel;

    if (model && model->isActive()) {
        that->updatePanelState(WaylandInputPanel::InputPanelHidden);
        model->sendLeft();
    }

    // Case when deactivate() & destroy() is not called. ex) MaliitServer is shutdown.
    if (resource == that->m_resource)
        that->m_resource = 0;

    free(resource);
    that->m_resourceCount--;

    that->maybeDestroy();
}

void WaylandInputMethodContext::destroy(struct wl_client *client, struct wl_resource *resource)
{
    Q_UNUSED(client);
    wl_resource_destroy(resource);
}

void WaylandInputMethodContext::commitString(struct wl_client *client, struct wl_resource *resource, uint32_t serial, const char *text)
{
    Q_UNUSED(client);
    WaylandInputMethodContext* that = static_cast<WaylandInputMethodContext*>(resource->data);
    if (that->m_textModel)
        that->m_textModel->commitString(serial, text);
}

void WaylandInputMethodContext::preEditString(struct wl_client *client, struct wl_resource *resource, uint32_t serial, const char *text, const char* commit)
{
    Q_UNUSED(client);
    WaylandInputMethodContext* that = static_cast<WaylandInputMethodContext*>(resource->data);
    if (that->m_textModel)
        that->m_textModel->preEditString(serial, text, commit);
}

void WaylandInputMethodContext::preEditStyling(struct wl_client *client, struct wl_resource *resource, uint32_t serial, uint32_t index, uint32_t length, uint32_t style)
{
    Q_UNUSED(client);
    WaylandInputMethodContext* that = static_cast<WaylandInputMethodContext*>(resource->data);
    if (that->m_textModel)
        that->m_textModel->preEditStyling(serial, index, length, style);
}

void WaylandInputMethodContext::preEditCursor(struct wl_client *client, struct wl_resource *resource, uint32_t serial, int32_t index)
{
    Q_UNUSED(client);
    WaylandInputMethodContext* that = static_cast<WaylandInputMethodContext*>(resource->data);
    if (that->m_textModel)
        that->m_textModel->preEditCursor(serial, index);
}

void WaylandInputMethodContext::deleteSurroundingText(struct wl_client *client, struct wl_resource *resource, uint32_t serial, int32_t index, uint32_t length)
{
    Q_UNUSED(client);
    WaylandInputMethodContext* that = static_cast<WaylandInputMethodContext*>(resource->data);
    if (that->m_textModel)
        that->m_textModel->deleteSurroundingText(serial, index, length);
}

void WaylandInputMethodContext::cursorPosition(struct wl_client *client, struct wl_resource *resource, uint32_t serial, int32_t index, int32_t anchor)
{
    Q_UNUSED(client);
    WaylandInputMethodContext* that = static_cast<WaylandInputMethodContext*>(resource->data);
    if (that->m_textModel)
        that->m_textModel->cursorPosition(serial, index, anchor);
}

void WaylandInputMethodContext::modifiersMap(struct wl_client *client, struct wl_resource *resource, struct wl_array *map)
{
    Q_UNUSED(client);
    WaylandInputMethodContext* that = static_cast<WaylandInputMethodContext*>(resource->data);
    if (that->m_textModel)
        that->m_textModel->modifiersMap(map);
}

void WaylandInputMethodContext::keySym(struct wl_client *client, struct wl_resource *resource, uint32_t serial, uint32_t time, uint32_t sym, uint32_t state, uint32_t modifiers)
{
    Q_UNUSED(client);
    WaylandInputMethodContext* that = static_cast<WaylandInputMethodContext*>(resource->data);
    if (that->m_textModel)
        text_model_send_keysym(that->m_textModel->handle(), serial, time, sym, state, modifiers);
}


void WaylandInputMethodContext::grabKeyboard(struct wl_client *client, struct wl_resource *resource, uint32_t id)
{
    WaylandInputMethodContext* that = static_cast<WaylandInputMethodContext*>(resource->data);

    // NOTE: We assign the resource of the default input device for the grab resource of input method context.
    //       The input method context will communicate with the client through the default input device.
    QtWayland::InputDevice* p_default_device = that->m_inputMethod->compositor()->defaultInputDevice()->handle();

    // NOTE: Currenlty, version number is hardcoded as '1'; if any better idea for the value, please replace it.
    that->m_grabResource = p_default_device->keyboardDevice()->add(client, id, 1);

    that->grabKeyboardImpl();
}

void WaylandInputMethodContext::focused(QtWayland::Surface* surface)
{
    Q_ASSERT(m_keyboard);
    if (!surface)
        deactivate();
    else if (m_grabResource)
        m_keyboard->sendKeyModifiers(m_grabResource, 1);
}

void WaylandInputMethodContext::key(uint32_t serial, uint32_t time, uint32_t key, uint32_t state)
{
    Q_ASSERT(m_keyboard);
    if (m_grabResource)
        m_keyboard->send_key(m_grabResource->handle, serial, time, key, state);
}

void WaylandInputMethodContext::modifiers(uint32_t serial, uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked, uint32_t group)
{
    Q_ASSERT(m_keyboard);
    if (m_grabResource)
        m_keyboard->send_modifiers(m_grabResource->handle, serial, mods_depressed, mods_latched, mods_locked, group);
}

void WaylandInputMethodContext::key(struct wl_client *client, struct wl_resource *resource, uint32_t serial, uint32_t time, uint32_t key, uint32_t state)
{
    Q_UNUSED(client);
    Q_UNUSED(resource);
    Q_UNUSED(serial);
    Q_UNUSED(time);
    Q_UNUSED(key);
    Q_UNUSED(state);
}

void WaylandInputMethodContext::modifiers(struct wl_client *client, struct wl_resource *resource, uint32_t serial, uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked, uint32_t group)
{
    Q_UNUSED(client);
    Q_UNUSED(resource);
    Q_UNUSED(serial);
    Q_UNUSED(mods_depressed);
    Q_UNUSED(mods_latched);
    Q_UNUSED(mods_locked);
    Q_UNUSED(group);
}

WaylandInputMethodContext::~WaylandInputMethodContext()
{
}

void WaylandInputMethodContext::maybeDestroy()
{
    //WaylandInputMethodContext will be destroyed now or soon.
    //So release grab also.
    releaseGrab();

    /* WaylandInputMethodContext has same life cycle with WaylandTextModel.
       But it has to be alive until all resources are destroyed. */
    if (m_textModel || m_resourceCount)
        return;

    delete this;
}

void WaylandInputMethodContext::deactivate()
{
    qDebug() << "model" << m_textModel;
    if (m_textModel && m_textModel->isActive()) {
        cleanup();
        updatePanelState(WaylandInputPanel::InputPanelHidden);
        m_textModel->sendLeft();
    }
}

void WaylandInputMethodContext::updatePanelState(const WaylandInputPanel::InputPanelState state) const
{
    if (!m_textModel)
        return;

    m_textModel->sendInputPanelState(state);
}

void WaylandInputMethodContext::updatePanelSize(const QRect &rect) const
{
    if (!m_textModel)
        return;

    m_textModel->sendInputPanelRect(rect);
}

void WaylandInputMethodContext::activateTextModel()
{
    qDebug() << "model" << m_textModel;

    if (!m_inputMethod->inputMethodManager()->requestInputMethod()) {
        connect(m_inputMethod->inputMethodManager(), SIGNAL(inputMethodAvaliable()),
                this, SLOT(continueTextModelActivation()),
                static_cast<Qt::ConnectionType>(Qt::QueuedConnection | Qt::UniqueConnection));

        qWarning() << "No input panel available, try to start MaliitServer";
        return;
    }

    if (!m_inputMethod->handle()) {
        qWarning() << "No input panel available, try to start MaliitServer";
        return;
    }

    if (m_inputMethod->active()) {
        qWarning() << "InputMethodContext is already activated. it will be deactivated.";
        m_inputMethod->deactivate();
    }

    m_resourceCount++;
    m_resource = (wl_resource*)calloc(1, sizeof(wl_resource));
    m_resource->destroy = WaylandInputMethodContext::destroyInputMethodContext;
    m_resource->object.id = 0;
    m_resource->object.interface = &input_method_context_interface;
    m_resource->object.implementation = (void (**)(void)) &inputMethodContextImplementation;
    m_resource->data = this;
    wl_signal_init(&m_resource->destroy_signal);

    static int serial = 0; //TODO
    wl_client_add_resource(m_inputMethod->handle()->client, m_resource);
    input_method_send_activate(m_inputMethod->handle(), m_resource, serial++);

    m_activated = true;

    emit activated();
    m_textModel->sendEntered();
}

void WaylandInputMethodContext::continueTextModelActivation()
{
    disconnect(m_inputMethod->inputMethodManager(), SIGNAL(inputMethodAvaliable()),
        this, SLOT(continueTextModelActivation()));

    activateTextModel();
}

void WaylandInputMethodContext::deactivateTextModel()
{
    WaylandTextModel* textModel = qobject_cast<WaylandTextModel*>(sender());
    qDebug() << "model" << textModel;
    cleanup();
}

void WaylandInputMethodContext::destroyTextModel()
{
    WaylandTextModel* textModel = qobject_cast<WaylandTextModel*>(sender());
    qDebug() << "model" << textModel;
    m_textModel = NULL;
    cleanup();

    maybeDestroy();
}

void WaylandInputMethodContext::updateContentType(uint32_t hint, uint32_t purpose)
{
    if (m_resource) {
        qDebug() << hint << purpose;
        input_method_context_send_content_type(m_resource, hint, purpose);
    }
}

void WaylandInputMethodContext::updateEnterKeyType(uint32_t enter_key_type)
{
    if (m_resource) {
        qDebug() << enter_key_type;
        input_method_context_send_enter_key_type(m_resource, enter_key_type);
    }
}

void WaylandInputMethodContext::updateSurroundingText(const QString& text, uint32_t cursor, uint32_t anchor)
{
    if (m_resource) {
        input_method_context_send_surrounding_text(m_resource, text.toUtf8().data(), cursor, anchor);
    }
}

void WaylandInputMethodContext::resetContext(uint32_t serial)
{
    if (m_resource) {
        input_method_context_send_reset(m_resource, serial);
    }
}

void WaylandInputMethodContext::commit()
{
    if (m_resource) {
        input_method_context_send_commit(m_resource);
    }
}

void WaylandInputMethodContext::invokeAction(uint32_t button, uint32_t index)
{
    if (m_resource) {
        input_method_context_send_invoke_action(m_resource, button, index);
    }
}

void WaylandInputMethodContext::maxTextLengthTextModel(uint32_t length)
{
    if (m_resource) {
        input_method_context_send_max_text_length(m_resource, length);
    }
}

void WaylandInputMethodContext::platformDataModel(const QString& text)
{
    if (m_resource) {
        input_method_context_send_platform_data(m_resource, text.toUtf8().data());
    }
}

void WaylandInputMethodContext::cleanup()
{
    disconnect(m_inputMethod->inputMethodManager(), SIGNAL(inputMethodAvaliable()),
        this, SLOT(continueTextModelActivation()));

    /* Prevent from sending deactivate repeatedly. Otherwise client can be stuck
       after reqeust to destroy input_method_context. */
    if (!m_activated)
        return;

    m_activated = false;
    if (m_resource && m_inputMethod->handle()) {
        input_method_send_deactivate(m_inputMethod->handle(), m_resource);
        emit deactivated();
        // It is supposed that the resource will be destroyed soon.
        // It is better to send nothing through the resource
        m_resource = 0;
    }
    releaseGrab();
}

void WaylandInputMethodContext::releaseGrab()
{
    releaseGrabImpl();
    m_grabResource = NULL;
}

void WaylandInputMethodContext::grabKeyboardImpl()
{
    if (m_grabbed)
        return;

    WaylandInputMethod *inputMethod = m_inputMethod;
#ifdef MULTIINPUT_SUPPORT
    WebOSCoreCompositor *compositor = static_cast<WebOSCoreCompositor*>(inputMethod->compositor());
    QList<QWaylandInputDevice *> devices = compositor->inputDevices();
    foreach (QWaylandInputDevice *device, devices) {
        QtWayland::Keyboard* p_keyboard = device->handle()->keyboardDevice();
        if (p_keyboard) {
            p_keyboard->startGrab(this);

            int devId = static_cast<WebOSInputDevice*>(device)->id();
            compositor->inputManager()->setGrabStatus(devId, true);
        }
    }
#else
    QtWayland::InputDevice* p_device = inputMethod->compositor()->defaultInputDevice()->handle();
    QtWayland::Keyboard* p_keyboard = p_device->keyboardDevice();
    p_keyboard->startGrab(this);
#endif

    m_grabbed = true;
}

void WaylandInputMethodContext::releaseGrabImpl()
{
    if (!m_grabbed)
        return;

#ifdef MULTIINPUT_SUPPORT
    WebOSCoreCompositor *compositor = static_cast<WebOSCoreCompositor*>(m_inputMethod->compositor());
    QList<QWaylandInputDevice *> devices = compositor->inputDevices();
    foreach (QWaylandInputDevice *device, devices) {
        QtWayland::Keyboard* p_keyboard = device->handle()->keyboardDevice();
        if (p_keyboard) {
            p_keyboard->endGrab();

            int devId = static_cast<WebOSInputDevice*>(device)->id();
            compositor->inputManager()->setGrabStatus(devId, false);
        }
    }
#else
    QtWayland::InputDevice* p_device = m_inputMethod->compositor()->defaultInputDevice()->handle();
    QtWayland::Keyboard* p_keyboard = p_device->keyboardDevice();
    p_keyboard->endGrab();
#endif

    m_grabbed = false;
}
