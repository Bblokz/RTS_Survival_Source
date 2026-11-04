"""Preview renders of the split engine decks (build_engine_decks.py output) for review before importing.

Run headless:  blender.exe -b --factory-startup --python render_engine_decks.py -- <decks_dir> <texture_dir> <out_dir> [uv_index]
<texture_dir> holds <Tank>_*.tga diffuse textures exported from Unreal. Renders per tank: the wreck with the deck in
place, and the deck blown up and tilted above the open engine bay.
"""
import glob
import math
import os
import sys

import bpy
from mathutils import Euler, Vector

TANKS = ['Tiger1', 'PantherD', 'Tiger2']
# Deck lifted (m) and tilted (degrees about X, Y) in the "thrown" view.
LIFT = Vector((-0.4, 1.9, 1.4))
TILT = (math.radians(28), math.radians(-18), math.radians(12))


def setup_materials(texture_path, uv_name):
    image = bpy.data.images.load(texture_path) if texture_path else None
    for material in bpy.data.materials:
        material.use_nodes = True
        nodes = material.node_tree.nodes
        bsdf = nodes.get('Principled BSDF') or nodes.new('ShaderNodeBsdfPrincipled')
        if material.name == 'M_EngineBay':
            bsdf.inputs['Base Color'].default_value = (0.05, 0.035, 0.025, 1.0)
            bsdf.inputs['Roughness'].default_value = 0.9
            continue
        bsdf.inputs['Roughness'].default_value = 0.75
        if image is None:
            continue
        tex = nodes.new('ShaderNodeTexImage')
        tex.image = image
        uv = nodes.new('ShaderNodeUVMap')
        uv.uv_map = uv_name
        material.node_tree.links.new(uv.outputs['UV'], tex.inputs['Vector'])
        material.node_tree.links.new(tex.outputs['Color'], bsdf.inputs['Base Color'])
        nodes.active = tex


def render(path, camera_location, target):
    scene = bpy.context.scene
    camera = scene.camera
    camera.location = camera_location
    camera.rotation_euler = (target - camera_location).to_track_quat('-Z', 'Y').to_euler()
    scene.render.filepath = path
    bpy.ops.render.render(write_still=True)


def main():
    args = sys.argv[sys.argv.index('--') + 1:]
    decks_dir, texture_dir, out_dir = args[:3]
    uv_index = int(args[3]) if len(args) > 3 else 0
    os.makedirs(out_dir, exist_ok=True)
    for key in TANKS:
        bpy.ops.wm.open_mainfile(filepath=os.path.join(decks_dir, key + '_EngineDeck.blend'))
        scene = bpy.context.scene
        meshes = [obj for obj in scene.objects if obj.type == 'MESH']
        deck = next(obj for obj in meshes if obj.name.endswith('_EngineDeck'))
        hull = next(obj for obj in meshes if obj is not deck)
        uv_name = hull.data.uv_layers[uv_index].name
        textures = glob.glob(os.path.join(texture_dir, key + '_*.tga'))
        setup_materials(textures[0] if textures else None, uv_name)

        scene.render.engine = 'CYCLES'
        scene.cycles.samples = 48
        scene.cycles.use_denoising = True
        scene.render.resolution_x, scene.render.resolution_y = 1000, 760
        world = bpy.data.worlds.new('World')
        world.use_nodes = True
        world.node_tree.nodes['Background'].inputs['Color'].default_value = (0.55, 0.6, 0.68, 1.0)
        world.node_tree.nodes['Background'].inputs['Strength'].default_value = 0.9
        scene.world = world
        sun = bpy.data.objects.new('Sun', bpy.data.lights.new('Sun', 'SUN'))
        sun.data.energy = 3.5
        sun.rotation_euler = Euler((math.radians(40), math.radians(15), math.radians(-60)))
        scene.collection.objects.link(sun)
        camera = bpy.data.objects.new('Camera', bpy.data.cameras.new('Camera'))
        camera.data.lens = 50
        scene.collection.objects.link(camera)
        scene.camera = camera
        bpy.ops.mesh.primitive_plane_add(size=40, location=(0, 0, -0.25))

        deck_centre = sum((deck.matrix_world @ Vector(corner) for corner in deck.bound_box), Vector()) / 8
        view_from = deck_centre + Vector((-5.6, -4.0, 5.6))
        render(os.path.join(out_dir, key + '_1_in_place.png'), view_from, deck_centre)
        deck.location = LIFT
        deck.rotation_mode = 'XYZ'
        # Tilt about the deck's own centre.
        deck.matrix_world = (deck.matrix_world.Translation(deck_centre + LIFT) @ Euler(TILT).to_matrix().to_4x4()
                             @ deck.matrix_world.Translation(-deck_centre))
        render(os.path.join(out_dir, key + '_2_thrown.png'), view_from + Vector((0, -1.0, 1.0)), deck_centre + Vector((0, 0.9, 0.5)))


if __name__ == '__main__':
    main()
