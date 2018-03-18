extends Node2D

var tex

func _draw():
	var svg = SVG.new()
	svg.load("res://hippo.svg")
	var img = svg.rasterize(128, 128, 0, 0, 200)
	tex = ImageTexture.new()
	tex.create_from_image(img)
	draw_texture(tex, Vector2(100, 100))
