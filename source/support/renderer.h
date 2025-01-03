//
// Copyright (C) YuqiaoZhang(HanetakaChou)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published
// by the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
//

#ifndef _RENDERER_H_
#define _RENDERER_H_ 1

extern class renderer *renderer_init(void *wsi_connection);
extern void renderer_destroy(class renderer *renderer);
extern void renderer_attach_window(class renderer *renderer, void *wsi_window);
extern void renderer_on_window_resize(class renderer *renderer);
extern void renderer_dettach_window(class renderer *renderer);
extern void renderer_draw(class renderer *renderer);

#endif