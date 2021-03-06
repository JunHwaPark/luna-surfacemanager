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

#ifndef WAYLANDINPUTMETHOD_H
#define WAYLANDINPUTMETHOD_H

#include <QObject>

#include <wayland-server.h>
#include <wayland-input-method-server-protocol.h>

class QWaylandCompositor;
class WaylandTextModelFactory;
class WaylandInputMethodContext;
class WaylandInputPanel;
class WaylandInputMethodManager;
/*!
 * Talks with the input method sitting in the VKB
 * process.
 */
class WaylandInputMethod : public QObject {

    Q_OBJECT

    Q_PROPERTY(bool active READ active)
    Q_PROPERTY(bool allowed READ allowed WRITE setAllowed NOTIFY allowedChanged)

public:
    WaylandInputMethod(QWaylandCompositor* compositor);
    ~WaylandInputMethod();

    static void bind(struct wl_client *client, void *data, uint32_t version, uint32_t id);
    static void destroyInputMethod(struct wl_resource* resource);

    wl_resource* handle() const { return m_resource; }
    QWaylandCompositor* compositor() const { return m_compositor; }
    WaylandInputPanel* inputPanel() { return m_inputPanel; }
    WaylandInputMethodManager* inputMethodManager() const { return m_inputMethodManager; }
    Q_INVOKABLE bool active() const { return m_activeContext != NULL; }
    bool allowed() const { return m_allowed; };
    void setAllowed(bool allowed);

public slots:
    void deactivate();

    void contextActivated();
    void contextDeactivated();

signals:
    void inputMethodActivated();
    void inputMethodDeactivated();

    void inputMethodBound(bool);

    void allowedChanged();

private:

    QWaylandCompositor* m_compositor;
    WaylandTextModelFactory* m_factory;
    struct wl_resource* m_resource;

    WaylandInputMethodContext* m_activeContext;
    WaylandInputPanel* m_inputPanel;
    WaylandInputMethodManager* m_inputMethodManager;
    bool m_allowed;
};

#endif //WAYLANDINPUTMETHOD_H
