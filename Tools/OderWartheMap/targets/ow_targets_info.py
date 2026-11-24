"""Texts of the Werk target concepts (plain Python: used by the Blender builders and the PIL sheet composer)."""

TM_INFO = {
    'TM_A': ('Lintel drop', 'The lintel breaks in a V and drops into the mouth; a rubble cone chokes it and spills '
                            'onto the apron. Doors stay.'),
    'TM_B': ('Berm slide', 'The earth berm slumps forward over the headwall and pours out of the mouth as an earth '
                           'tongue with a few concrete blocks in it.'),
    'TM_C': ('Headwall blown out', 'The face around the opening is blasted ragged, the lintel sags; a wide low '
                                   'debris fan and a scorch mark in front.'),
    'TM_D': ('Cave-in + tilt', 'The roof behind the headwall caves in (crater in the berm), the headwall leans '
                               'forward and a fallen slab seals the mouth.'),
    'TM_E': ('Doors down, block wall', 'The blast doors are torn off and lie flat; the mouth is filled to the '
                                       'top with big blocks; scorched apron.'),
    'TM_F': ('Half collapse', 'Only one side fails: the lintel drops on the left, a diagonal rubble slope; the '
                              'right side is choked waist-high.'),
    'TM_G': ('Whole tunnel collapsed', 'The cut-and-cover roof caves in along the whole tunnel (a long trough of '
                                       'broken slabs in the berm), the lintel and coping are down across the full '
                                       'width, the wing walls crumble, the doors are crushed and the mouth is packed '
                                       'with rubble.'),
}


CE_INFO = {
    'CE_A': ('Gas-lock porch (Vorbau)', 'A 4 x 5 m concrete porch built onto the Werk door: side door, sod roof, a '
                                        'sandbag blast wall, net over the forecourt. Destroyed: roof slab down, '
                                        'walls split.'),
    'CE_B': ('Sandbag chicane + net', 'A new armoured door in the Werkeingang behind a sandbag chicane, all under '
                                      'a camouflage net. Destroyed: door blown out, bags scattered, net burnt.'),
    'CE_C': ('Timbered trench', 'A 10 m zig-zag approach trench with timber revetment and net strips, ending at '
                                'an armoured door. Destroyed: banks caved in, door gone.'),
    'CE_D': ('Stairwell descent', 'A separate concrete stair shaft (Treppenabgang) going down in front of the '
                                  'door, open hatch, air pipe, net. Destroyed: shaft choked with rubble.'),
    'CE_E': ('Vehicle ramp + double door', 'An earth-covered block with an armoured double door at the end of a '
                                           'walled ramp. Destroyed: roof caved, door torn out.'),
    'CE_F': ('Hatch + vents', 'An armoured escape hatch, two mushroom air vents and a periscope under a low net: '
                              'the only visible sign of the command post. Destroyed: crater, bent pipes.'),
}


JT_INFO = {
    'JT_A': ('Cage on the mast + cellar', 'A wire-cage jammer antenna clamped halfway up the Werk mast, fed from a '
                                          'sod-covered transmitter cellar. Destroyed: cage torn down, cellar caved.'),
    'JT_B': ('Dipole curtain', 'A curtain of dipoles strung between two new 10 m poles behind the mast, fed from '
                               'a dugout. Destroyed: a pole snapped, the curtain down.'),
    'JT_C': ('Yagi stack on the mast', 'Three stacked Yagi arrays on a rotator above the mast top, cable down to '
                                       'a cellar. Destroyed: the stack bent and hanging off the mast.'),
    'JT_D': ('Bermed vault + whips', 'A low concrete transmitter vault in an earth bank with four tall whip '
                                     'antennas on its corners and a net. Destroyed: roof cracked, whips bent.'),
    'JT_E': ('Trailer van in a revetment', 'A box-body jammer trailer with a folding dipole mast, dug into a U '
                                           'of earth under a net. Destroyed: burnt-out shell.'),
    'JT_F': ('Umbrella wires', 'Wires from the mast top to nine short posts (an umbrella antenna) and a small '
                               'tuning cellar. Destroyed: posts down, wires slack.'),
}


RE_INFO = {
    'RE_A': ('Inner rail gate', 'An armoured sliding gate across the tunnel 5 m inside the mouth, where the rails '
                                'run in. Hard to see from above. Destroyed: gate blown in.'),
    'RE_B': ('Camouflage curtain', 'A net curtain hung from the lintel closes the mouth, plus a loading dock on '
                                   'the apron. Destroyed: curtain burnt away, dock wrecked.'),
    'RE_C': ('Platform + armoured rail car', 'A loading platform on the apron and an armoured rail car parked '
                                             'under a net in front of the mouth. Destroyed: burnt car off the rails.'),
    'RE_D': ('Signal box + gantry', 'A signal box dug into the berm beside the portal and a signal gantry over '
                                    'the rails. Destroyed: box caved, gantry fallen.'),
    'RE_E': ('Narrow-gauge side adit', 'A second, small timbered adit beside the portal with a narrow-gauge '
                                       'track and a cart. Destroyed: adit collapsed.'),
    'RE_F': ('The tunnel mouth itself', 'No extra piece: on the command post Werk the tunnel mouth Blueprint is '
                                        'also the rail target (heavy weapons only, collapses as chosen above).'),
}


GROUPS = {
    'tunnel': dict(
        title='Oder-Warthe: tunnel mouth collapse (pick one)',
        intro='Intact = the existing SM_OW_TunnelPortal (left of every pair). Only heavy hits hurt it; when it dies it '
              'swaps to the collapsed mesh, stays in the world, blocks the mouth and stops that Werk’s waves. Same '
              'kit concrete, earth and steel as the portal. Pivot, footprint and yaw unchanged.',
        options=TM_INFO),
    'command': dict(
        title='Oder-Warthe: command bunker entrance (pick one)',
        intro='Placed at the Werk’s Entrance socket (the Werkeingang door; true height and facing on all three '
              'designs), +X out of the door. Shown against a generic Werkeingang wall. Hidden until the fix; the '
              'destroyed piece stays (the archive squad enters it later). HQ_Command lies at z 0 and, on the '
              'Peilturm, inside the tower, so it cannot seat a piece.',
        options=CE_INFO),
    'jammer': dict(
        title='Oder-Warthe: jammer transmitter (pick one)',
        intro='HQ_Transmitter is the foot of the Werk mast (SM_OW_WerkMast stands there at all five Werke), so the '
              'jammer is built onto or beside that mast, clear of its three guy anchors. The mast itself stays. '
              'Hidden until the fix; the wreck stays when destroyed.',
        options=JT_INFO),
    'rail': dict(
        title='Oder-Warthe: tunnel railway entrance (pick one)',
        intro='HQ_Rail is exactly the tunnel portal’s spot on every design, so the rail target shares the 22 m '
              'portal footprint with the collapsible tunnel mouth. Options A-E are add-ons to the portal; F makes the '
              'tunnel mouth itself the third target. Hidden until the fix (except F).',
        options=RE_INFO),
}
