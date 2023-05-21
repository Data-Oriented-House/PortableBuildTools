package PortableBuildTools

import "widgets"

Rect :: widgets.Rect

rect_cut_top :: proc(r: ^Rect, a: int) -> (b: Rect) {
	b.x = r.x
	b.w = r.w
	b.y = r.y

	r.h = max(r.h - a, 0)
	r.y += a if r.h > 0 else 0
	b.h = r.y - b.y
	return
}

rect_cut_bottom :: proc(r: ^Rect, a: int) -> (b: Rect) {
	b.x = r.x
	b.w = r.w

	b.h = a if r.h > a else r.h
	r.h = max(r.h - a, 0)
	b.y = r.y + r.h
	return
}

rect_cut_left :: proc(r: ^Rect, a: int) -> (b: Rect) {
	b.y = r.y
	b.h = r.h
	b.x = r.x

	r.w = max(r.w - a, 0)
	r.x += a if r.w > 0 else 0
	b.w = r.x - b.x
	return
}

rect_cut_right :: proc(r: ^Rect, a: int) -> (b: Rect) {
	b.y = r.y
	b.h = r.h

	b.w = a if r.w > a else r.w
	r.w = max(r.w - a, 0)
	b.x = r.x + r.w
	return
}

rect_cut_margins :: proc(r: ^Rect, a: int) {
	rect_cut_top(r, a)
	rect_cut_bottom(r, a)
	rect_cut_left(r, a)
	rect_cut_right(r, a)
}
