/*************************************************************************/
/*  multiplayer_editor_plugin.cpp                                        */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2022 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2022 Godot Engine contributors (cf. AUTHORS.md).   */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#include "multiplayer_editor_plugin.h"

#include "../multiplayer_synchronizer.h"
#include "editor_network_profiler.h"
#include "replication_editor.h"

#include "editor/editor_node.h"

bool MultiplayerEditorDebugger::has_capture(const String &p_capture) const {
	return p_capture == "multiplayer";
}

bool MultiplayerEditorDebugger::capture(const String &p_message, const Array &p_data, int p_session) {
	ERR_FAIL_COND_V(!profilers.has(p_session), false);
	EditorNetworkProfiler *profiler = profilers[p_session];
	if (p_message == "multiplayer:rpc") {
		MultiplayerDebugger::RPCFrame frame;
		frame.deserialize(p_data);
		for (int i = 0; i < frame.infos.size(); i++) {
			profiler->add_node_frame_data(frame.infos[i]);
		}
		return true;

	} else if (p_message == "multiplayer:bandwidth") {
		ERR_FAIL_COND_V(p_data.size() < 2, false);
		profiler->set_bandwidth(p_data[0], p_data[1]);
		return true;
	}
	return false;
}

void MultiplayerEditorDebugger::_profiler_activate(bool p_enable, int p_session_id) {
	Ref<EditorDebuggerSession> session = get_session(p_session_id);
	ERR_FAIL_COND(session.is_null());
	session->toggle_profiler("multiplayer", p_enable);
	session->toggle_profiler("rpc", p_enable);
}

void MultiplayerEditorDebugger::setup_session(int p_session_id) {
	Ref<EditorDebuggerSession> session = get_session(p_session_id);
	ERR_FAIL_COND(session.is_null());
	EditorNetworkProfiler *profiler = memnew(EditorNetworkProfiler);
	profiler->connect("enable_profiling", callable_mp(this, &MultiplayerEditorDebugger::_profiler_activate).bind(p_session_id));
	profiler->set_name(TTR("Network Profiler"));
	session->add_session_tab(profiler);
	profilers[p_session_id] = profiler;
}

MultiplayerEditorPlugin::MultiplayerEditorPlugin() {
	repl_editor = memnew(ReplicationEditor);
	button = EditorNode::get_singleton()->add_bottom_panel_item(TTR("Replication"), repl_editor);
	button->hide();
	repl_editor->get_pin()->connect("pressed", callable_mp(this, &MultiplayerEditorPlugin::_pinned));
	debugger.instantiate();
}

MultiplayerEditorPlugin::~MultiplayerEditorPlugin() {
}

void MultiplayerEditorPlugin::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			get_tree()->connect("node_removed", callable_mp(this, &MultiplayerEditorPlugin::_node_removed));
			add_debugger_plugin(debugger);
		} break;
		case NOTIFICATION_EXIT_TREE: {
			remove_debugger_plugin(debugger);
		}
	}
}

void MultiplayerEditorPlugin::_node_removed(Node *p_node) {
	if (p_node && p_node == repl_editor->get_current()) {
		repl_editor->edit(nullptr);
		if (repl_editor->is_visible_in_tree()) {
			EditorNode::get_singleton()->hide_bottom_panel();
		}
		button->hide();
		repl_editor->get_pin()->set_pressed(false);
	}
}

void MultiplayerEditorPlugin::_pinned() {
	if (!repl_editor->get_pin()->is_pressed()) {
		if (repl_editor->is_visible_in_tree()) {
			EditorNode::get_singleton()->hide_bottom_panel();
		}
		button->hide();
	}
}

void MultiplayerEditorPlugin::edit(Object *p_object) {
	repl_editor->edit(Object::cast_to<MultiplayerSynchronizer>(p_object));
}

bool MultiplayerEditorPlugin::handles(Object *p_object) const {
	return p_object->is_class("MultiplayerSynchronizer");
}

void MultiplayerEditorPlugin::make_visible(bool p_visible) {
	if (p_visible) {
		button->show();
		EditorNode::get_singleton()->make_bottom_panel_item_visible(repl_editor);
	} else if (!repl_editor->get_pin()->is_pressed()) {
		if (repl_editor->is_visible_in_tree()) {
			EditorNode::get_singleton()->hide_bottom_panel();
		}
		button->hide();
	}
}
