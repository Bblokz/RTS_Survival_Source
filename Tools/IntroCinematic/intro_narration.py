"""Narration rows of the 1976 intro cinematic: one wave per subtitle row.

Source of truth: the "Full narration" block of Source/Docs/Campaign/IntroCinematicScript.md.
`spoken` is the accent spelling for the voice pass, `subtitle` the same words in plain English.
Rows 9.5 and 9.6 are a PLACEHOLDER (the old closing line) until the user finishes the closing paragraph.
"""

# (row id, shot id, spoken, subtitle, placeholder)
ROWS = [
    ("1_1", "1.1", "Kommandant, it is good to have you back.", "Kommandant, it is good to have you back.", False),
    ("1_2", "1.2", "It is my job to brief you on what we know today.", "It is my job to brief you on what we know today.", False),
    ("2_1", "2.1", "In October of 1944, a month after we successfully launched our first V2 rocket against ze British,",
     "In October of 1944, a month after we successfully launched our first V2 rocket against the British,", False),
    ("2_2", "2.2", "Soviet spies managed to capture our blueprints", "Soviet spies managed to capture our blueprints", False),
    ("2_3", "2.3", "und created their own rocket program.", "and created their own rocket program.", False),
    ("3_1", "3.1", "1945... ze year it all came down. Ve were told ze atomic bomb was decades away.",
     "1945... the year it all came down. We were told the atomic bomb was decades away.", False),
    ("3_2", "3.2", "But God, were we wrong. Ze Soviets managed to win ze race for ze atom und zhey used our rocket technology to carry it.",
     "But God, were we wrong. The Soviets managed to win the race for the atom and they used our rocket technology to carry it.", False),
    ("3_5", "3.5", "Berlin was ze first target", "Berlin was the first target", False),
    ("3_6", "3.6", "und many followed.", "and many followed.", False),
    ("4_1", "4.1", "But as intel shows us now, ze Third Reich wasn't zheir only target.",
     "But as intel shows us now, the Third Reich wasn't their only target.", False),
    ("4_2", "4.2", "Und in May 1945 zhey struck across ze ocean, nuking ze facilities of ze Manhattan Project;",
     "And in May 1945 they struck across the ocean, nuking the facilities of the Manhattan Project;", False),
    ("4_3", "4.3", "clearly afraid of ze western coalition developing nuclear technology of zheir own!",
     "clearly afraid of the western coalition developing nuclear technology of their own!", False),
    ("5_1", "5.1", "Ze western powers could not accept defeat und in zheir dying breath zhey unleashed something else,",
     "The western powers could not accept defeat and in their dying breath they unleashed something else,", False),
    ("5_2", "5.2", "not a bomb but a silent weapon of some biological material.",
     "not a bomb but a silent weapon of some biological material.", False),
    ("5_3", "5.3", "Whatever ze hell it was, it worked too well. Millions died from direct contact with it.",
     "Whatever the hell it was, it worked too well. Millions died from direct contact with it.", False),
    ("5_4", "5.4", "As far as we know it was some kind of bio weapon for genocide.",
     "As far as we know it was some kind of bio-weapon for genocide.", False),
    ("6_1", "6.1", "Things escalated quickly after zhis", "Things escalated quickly after this", False),
    ("6_2", "6.2", "und we think ze Soviets nuked most parts of Eurasia as part of a larger war zhey fought against ze western coalition.",
     "and we think the Soviets nuked most parts of Eurasia as part of a larger war they fought against the western coalition.", False),
    ("6_2b", "6.2b", "Most of Europe today is destroyed due to ze Soviets using ze weapons on zheir own captured territories.",
     "Most of Europe today is destroyed due to the Soviets using the weapons on their own captured territories.", False),
    ("6_3", "6.3", "After zhis zhey moved far eastward und hid behind ze Ural Shield.",
     "After this they moved far eastward and hid behind the Ural Shield.", False),
    ("6_4", "6.4", "Ze Ural Shield is a massive wall of iron und concrete, spanning from ze northern to ze southern Russian border.",
     "The Ural Shield is a massive wall of iron and concrete, spanning from the northern to the southern Russian border.", False),
    ("7_1", "7.1", "Behind zat wall, something changed. Our intercepts no longer speak of one leader,",
     "Behind that wall, something changed. Our intercepts no longer speak of one leader,", False),
    ("7_2", "7.2", "but of a council of three. Ze Central Soviet Directorate.",
     "but of a council of three. The Central Soviet Directorate.", False),
    ("8_1", "8.1", "Ze atomic fire sank deep into ze bedrock", "The atomic fire sank deep into the bedrock", False),
    ("8_2", "8.2", "und caused a chemical reaction we now call Radixite.", "and caused a chemical reaction we now call Radixite.", False),
    ("8_3", "8.3", "Radixite is powerful as a resource, but it seems alive und very unstable.",
     "Radixite is powerful as a resource, but it seems alive and very unstable.", False),
    ("8_4", "8.4", "Zhese chemical reactions have caused massive heat waves, which dried up most bodies of water und turned many parts of ze landscape into a scorching desert.",
     "These chemical reactions have caused massive heat waves, which dried up most bodies of water and turned many parts of the landscape into a scorching desert.", False),
    ("9_1a", "9.1", "For reasons unknown, ze Soviets have stopped using nukes ever since.",
     "For reasons unknown, the Soviets have stopped using nukes ever since.", False),
    ("9_1b", "9.1", "And most of their outposts are now abandoned.", "And most of their outposts are now abandoned.", False),
    ("9_2", "9.2", "It has been thirty years.", "It has been thirty years.", False),
    ("9_3", "9.3", "Our survivors have adapted to ze heated landscape.", "Our survivors have adapted to the heated landscape.", False),
    ("9_4", "9.4", "Zhey think zhey sterilised ze earth. Zhey think we are dead.",
     "They think they sterilised the earth. They think we are dead.", False),
    ("9_5", "9.5", "It's time to show zhem", "It's time to show them", True),
    ("9_6", "9.6", "what survived ze fire!", "what survived the fire!", True),
]

VO_SCRATCH_DIR = "D:/UE5Projects/RTS_Survival/Saved/IntroCinematic/vo_scratch"
VO_ASSET_DIR = "/Game/RTS_Survival/Cinematics/Intro1976/Audio/VO_Scratch"


def wave_name(row_id):
    return "VO_Intro_%s" % row_id
