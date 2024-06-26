/*
Minetest
Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>
Copyright (C) 2018 nerzhul, Loic Blot <loic.blot@unix-experience.fr>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "gameui.h"
#include <irrlicht_changes/static_text.h>
#include <gettext.h>
#include "gui/mainmenumanager.h"
#include "gui/guiChatConsole.h"
#include "util/pointedthing.h"
#include "client.h"
#include "clientmap.h"
#include "fontengine.h"
#include "nodedef.h"
#include "profiler.h"
#include "renderingengine.h"
#include "version.h"
#include "log.h"

std::string get_irrlicht_device()
{	
	switch (RenderingEngine::get_raw_device()->getType()) 
	{
			case EIDT_WIN32: 
				return "WIN32";
			case EIDT_X11: 
				return "X11";
			case EIDT_OSX: 
				return "OSX";
			case EIDT_SDL: 
				return "SDL";
			case EIDT_ANDROID: 
				return "ANDROID";
			default: 
				return "Unknown";
	}
}

std::string get_videoDriver()
{
	return wide_to_utf8(RenderingEngine::get_video_driver()->getName()).c_str();
}

inline static const char *yawToDirectionString(int yaw)
{
	static const char *direction[4] =
		{"North +Z", "West -X", "South -Z", "East +X"};

	yaw = wrapDegrees_0_360(yaw);
	yaw = (yaw + 45) % 360 / 90;

	return direction[yaw];
}

GameUI::GameUI()
{
	if (guienv && guienv->getSkin())
		m_statustext_initial_color = guienv->getSkin()->getColor(gui::EGDC_BUTTON_TEXT);
	else
		m_statustext_initial_color = video::SColor(255, 0, 0, 0);

}
void GameUI::init()
{
	m_guitext_coords = gui::StaticText::add(guienv, L"", core::rect<s32>(0, 0, 0, 0),
		false, true, guiroot);
	// First line of debug text
	m_guitext = gui::StaticText::add(guienv, utf8_to_wide(PROJECT_NAME_C).c_str(),
		core::rect<s32>(0, 0, 0, 0), false, true, guiroot);

	// Second line of debug text
	m_guitext2 = gui::StaticText::add(guienv, L"", core::rect<s32>(0, 0, 0, 0), false,
		true, guiroot);

	// Chat text
	m_guitext_chat = gui::StaticText::add(guienv, L"", core::rect<s32>(0, 0, 0, 0),
		//false, false); // Disable word wrap as of now
		false, true, guiroot);
	
	u16 chat_font_size = g_settings->getU16("chat_font_size");
	if (chat_font_size != 0) {
		m_guitext_chat->setOverrideFont(g_fontengine->getFont(
			rangelim(chat_font_size, 5, 72), FM_Unspecified));
	}

	// Infotext of nodes and objects.
	// If in debug mode, object debug infos shown here, too.
	// Located on the left on the screen, below chat.
	u32 chat_font_height = m_guitext_chat->getActiveFont()->getDimension(L"Ay").Height;
	m_guitext_info = gui::StaticText::add(guienv, L"",
		// Size is limited; text will be truncated after 6 lines.
		core::rect<s32>(0, 0, 400, g_fontengine->getTextHeight() * 6) +
			v2s32(100, chat_font_height *
			(g_settings->getU16("recent_chat_messages") + 3)),
			false, true, guiroot);

	// Status text (displays info when showing and hiding GUI stuff, etc.)
	m_guitext_status = gui::StaticText::add(guienv, L"<Status>",
		core::rect<s32>(0, 0, 0, 0), false, false, guiroot);
	m_guitext_status->setVisible(false);

	// Profiler text (size is updated when text is updated)
	m_guitext_profiler = gui::StaticText::add(guienv, L"<Profiler>",
		core::rect<s32>(0, 0, 0, 0), false, false, guiroot);
		
	m_guitext_profiler->setOverrideFont(g_fontengine->getFont(
		g_fontengine->getDefaultFontSize() * 0.9f, FM_Mono));
	m_guitext_profiler->setVisible(false);
}

void GameUI::update(const RunStats &stats, Client *client, MapDrawControl *draw_control,
	const CameraOrientation &cam, const PointedThing &pointed_old,	
	const GUIChatConsole *chat_console, float dtime)
{
	v2u32 screensize = RenderingEngine::getWindowSize();
	const int fps_limit = (g_settings->getU64("fps_max"));
	LocalPlayer *player = client->getEnv().getLocalPlayer();
	s32 minimal_debug_height = 0;
	v3f player_position = player->getPosition();

	if (g_settings->getBool("show_coords")){
		std::ostringstream os(std::ios_base::binary);

		os << std::setprecision(1) << std::fixed
			<< (player_position.X / BS)
			<< ", " << (player_position.Y / BS)
			<< ", " << (player_position.Z / BS);
		setStaticText(m_guitext_coords, utf8_to_wide(os.str()).c_str());
		m_guitext_coords->setRelativePosition(core::rect<s32>(5, screensize.Y - 5 -  g_fontengine->getTextHeight(),
			screensize.X, screensize.Y));
	} else {
		m_guitext_coords -> setText(L"");
	}

	// Minimal debug text must only contain info that can't give a gameplay advantage
	if (m_flags.show_minimal_debug) {
	    const u16 fps = 1.0 / stats.dtime_jitter.avg;
	    m_drawtime_avg *= 0.95f;
	    m_drawtime_avg += 0.05f * (stats.drawtime / 1000);
	    v3f player_position = player->getPosition();

	    std::ostringstream os(std::ios_base::binary);
	    os << std::fixed
	      << PROJECT_NAME_C << " [" << g_version_hash << "]" << "[Minetest client]" << std::endl
	      << "FPS: " << fps << "/" << fps_limit << " | Driver: " << get_videoDriver() << std::endl
	      << "View range: " << (draw_control->range_all ? "All" : itos(draw_control->wanted_range)) << std::endl
	      << "Irrlicht device: " << get_irrlicht_device() << std::endl
	      << "Coords:  " << (player_position.X / BS) << ", " << (player_position.Y / BS) << ", " << (player_position.Z / BS) << std::endl
	      << "Yaw: " << (wrapDegrees_0_360(cam.camera_yaw)) << "° " << yawToDirectionString(cam.camera_yaw) << " | Pitch: " << (-wrapDegrees_180(cam.camera_pitch)) << "°" << std::endl
	      << "Seed: " << ((u64)client->getMapSeed()) << std::endl
	      << "Drawtime: " << m_drawtime_avg << "ms | Dtime jitter: " << (stats.dtime_jitter.max_fraction * 100.0) << "%" << std::endl
	      << "RTT: " << (client->getRTT() * 1000.0f) << "ms";

	    m_guitext->setRelativePosition(core::rect<s32>(5, 5, screensize.X, screensize.Y));

	    setStaticText(m_guitext, utf8_to_wide(os.str()).c_str());
	    
	   
	    minimal_debug_height = m_guitext->getTextHeight();
	}
	// Finally set the guitext visible depending on the flag
	m_guitext->setVisible(m_flags.show_minimal_debug);

	// Basic debug text also shows info that might give a gameplay advantage
	if (m_flags.show_basic_debug) {

		std::ostringstream os(std::ios_base::binary);
		os << std::setprecision(1) << std::fixed;
		if (pointed_old.type == POINTEDTHING_NODE) {
			ClientMap &map = client->getEnv().getClientMap();
			const NodeDefManager *nodedef = client->getNodeDefManager();
			MapNode n = map.getNode(pointed_old.node_undersurface);

			if (n.getContent() != CONTENT_IGNORE) {
				if (nodedef->get(n).name == "unknown") {
					os << "Pointed: <unknown node>";
				} else {
					os << "Pointed: " << nodedef->get(n).name;
				}
				os << ", param2: " << (u64) n.getParam2();
			}
		}

		m_guitext2->setRelativePosition(core::rect<s32>(5, 5 + minimal_debug_height,
				screensize.X, screensize.Y));

		setStaticText(m_guitext2, utf8_to_wide(os.str()).c_str());
	}

	m_guitext2->setVisible(m_flags.show_basic_debug);

	setStaticText(m_guitext_info, m_infotext.c_str());
	m_guitext_info->setVisible(m_flags.show_hud && g_menumgr.menuCount() == 0);

	static const float statustext_time_max = 1.5f;

	if (!m_statustext.empty()) {
		m_statustext_time += dtime;

		if (m_statustext_time >= statustext_time_max) {
			clearStatusText();
			m_statustext_time = 0.0f;
		}
	}

	setStaticText(m_guitext_status, m_statustext.c_str());
	m_guitext_status->setVisible(!m_statustext.empty());

	if (!m_statustext.empty()) {
		s32 status_width  = m_guitext_status->getTextWidth();
		s32 status_height = m_guitext_status->getTextHeight();
		s32 status_y = screensize.Y - 150;
		s32 status_x = (screensize.X - status_width) / 2;

		m_guitext_status->setRelativePosition(core::rect<s32>(status_x ,
			status_y - status_height, status_x + status_width, status_y));

		// Fade out
		video::SColor final_color = m_statustext_initial_color;
		final_color.setAlpha(0);
		video::SColor fade_color = m_statustext_initial_color.getInterpolated_quadratic(
			m_statustext_initial_color, final_color, m_statustext_time / statustext_time_max);
		m_guitext_status->setOverrideColor(fade_color);
		m_guitext_status->enableOverrideColor(true);
	}

	// Hide chat when disabled by server or when console is visible
	m_guitext_chat->setVisible(isChatVisible() && !chat_console->isVisible() && (player->hud_flags & HUD_FLAG_CHAT_VISIBLE));
}

void GameUI::initFlags()
{
	m_flags = GameUI::Flags();
	m_flags.show_minimal_debug = g_settings->getBool("show_debug");	
}

void GameUI::showMinimap(bool show)
{
	m_flags.show_minimap = show;
}

void GameUI::showTranslatedStatusText(const char *str)
{
	showStatusText(wstrgettext(str));
}

void GameUI::setChatText(const EnrichedString &chat_text, u32 recent_chat_count)
{
	m_guitext_chat->setBackgroundColor(video::SColor(90, 0, 0, 0));
	setStaticText(m_guitext_chat, chat_text);
	m_recent_chat_count = recent_chat_count;
}

void GameUI::updateChatSize()
{
	m_guitext_chat->setBackgroundColor(video::SColor(90, 0, 0, 0));
	// Update gui element size and position
	const v2u32 &window_size = RenderingEngine::getWindowSize();

	s32 chat_y = window_size.Y - 130 - m_guitext_chat->getTextHeight();

	if (m_flags.show_minimal_debug)
		chat_y += g_fontengine->getLineHeight();
	if (m_flags.show_basic_debug)
		chat_y += g_fontengine->getLineHeight();

	core::rect<s32> chat_size(10, chat_y, window_size.X - 20, 0);
	chat_size.LowerRightCorner.Y = std::min((s32)window_size.Y,
			m_guitext_chat->getTextHeight() + chat_y);

	if (chat_size == m_current_chat_size)
		return;
	m_current_chat_size = chat_size;

	m_guitext_chat->setRelativePosition(chat_size);
}


void GameUI::updateProfiler()
{
	if (m_profiler_current_page != 0) {
		std::ostringstream os(std::ios_base::binary);
		os << "   Profiler page " << (int)m_profiler_current_page <<
				", elapsed: " << g_profiler->getElapsedMs() << " ms)" << std::endl;

		int lines = g_profiler->print(os, m_profiler_current_page, m_profiler_max_page);
		++lines;

		EnrichedString str(utf8_to_wide(os.str()));
		str.setBackground(video::SColor(120, 0, 0, 0));
		setStaticText(m_guitext_profiler, str);

		core::dimension2d<u32> size = m_guitext_profiler->getOverrideFont()->
				getDimension(str.c_str());
		core::position2di upper_left(6, m_guitext->getTextHeight() * 2.5f);
		core::position2di lower_right = upper_left;
		lower_right.X += size.Width + 10;
		lower_right.Y += size.Height;

		m_guitext_profiler->setRelativePosition(core::rect<s32>(upper_left, lower_right));
	}

	m_guitext_profiler->setVisible(m_profiler_current_page != 0);
}

void GameUI::toggleChat(Client *client)
{
	if (client->getEnv().getLocalPlayer()->hud_flags & HUD_FLAG_CHAT_VISIBLE) {
		m_flags.show_chat = !m_flags.show_chat;
		if (m_flags.show_chat)
			showTranslatedStatusText("Chat shown");
		else
			showTranslatedStatusText("Chat hidden");
	} else {
		showTranslatedStatusText("Chat currently disabled by game or mod");
	}

}
void GameUI::toggleRenderMenu()
{
	m_flags.render_menu = !m_flags.render_menu;
	if (m_flags.render_menu){
		showTranslatedStatusText("Fast Menu shown");
		m_flags.show_minimal_debug = false;
		m_flags.show_basic_debug = false;
	} else {
		showTranslatedStatusText("Fast Menu hidden");
	}
}

void GameUI::toggleHud()
{
	m_flags.show_hud = !m_flags.show_hud;
	if (m_flags.show_hud)
		showTranslatedStatusText("HUD shown");
	else
		showTranslatedStatusText("HUD hidden");
}

void GameUI::toggleProfiler()
{
	m_profiler_current_page = (m_profiler_current_page + 1) % (m_profiler_max_page + 1);

	// FIXME: This updates the profiler with incomplete values
	updateProfiler();

	if (m_profiler_current_page != 0) {
		std::wstring msg = fwgettext("Profiler shown (page %d of %d)",
				m_profiler_current_page, m_profiler_max_page);
		showStatusText(msg);
	} else {
		showTranslatedStatusText("Profiler hidden");
	}
}


void GameUI::deleteFormspec()
{
	if (m_formspec) {
		m_formspec->drop();
		m_formspec = nullptr;
	}

	m_formname.clear();
}

void GameUI::Clear()
{
	
	if (m_guitext_chat) {
		m_guitext_chat->remove();
		m_guitext_chat = nullptr;
	}
	
	if (m_guitext) {
		m_guitext->remove();
		m_guitext = nullptr;
	}
	
	if (m_guitext2) {
		m_guitext2->remove();
		m_guitext2 = nullptr;
	}
	
	if (m_guitext_info) {
		m_guitext_info->remove();
		m_guitext_info = nullptr;
	}
		
	if (m_guitext_status) {
		m_guitext_status->remove();
		m_guitext_status = nullptr;
	}
		
	if (m_guitext_profiler) {
		m_guitext_profiler->remove();
		m_guitext_profiler = nullptr;
	}
	
	if (m_guitext_coords) {
		m_guitext_coords->remove();
		m_guitext_coords = nullptr;
	}	
}
