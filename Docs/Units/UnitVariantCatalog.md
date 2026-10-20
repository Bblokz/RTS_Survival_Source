# Unit variant catalog

Visual variant ideas for every German vehicle and squad-mounted vehicle in the mechanised, light, medium and heavy factories.

Each vehicle has six variants, lettered in order of how much the model changes:

- **A, B:** subtle. Same chassis and role, one visible part changes (gun, plates, mount).
- **C, D:** radical. The turret, casemate or weapon family changes; the silhouette reads as a different vehicle.
- **E, F:** medium. A clear visual change to the superstructure, gun or fittings, between the subtle and radical sets.

The implementation cap (`UnitVariants::MaxVariantsPerUnit`, currently 3) must be raised to 7 to fit base plus six variants. See the implementation notes at the end.

---

## Strike and Wehrmacht light vehicles

### Pz II Luchs
- **A Recon Luchs:** wide-view periscope cluster and a radio mast.
- **B Luchs 5 cm:** a longer 5 cm KwK 39 with a slim muzzle brake.
- **C Turretless Luchs:** the turret is removed for an open-topped troop carrier with a rear ramp.
- **D Luchs Panzerjäger:** a PaK 36 anti-tank gun on a pivot mount, turret removed.
- **E Flak Luchs:** a twin 20 mm mount on an open turret with a raised gun shield.
- **F Spaced armour:** side skirts and a spaced roof grille over the rear deck.

### Sdkfz 7 Flakvierling
- **A Quad AA focus:** the four barrels with a raised crew platform.
- **B Ground sweep:** the barrels are depressed low, with a wider ammunition box.
- **C Half-track rocket carrier:** a rack of 15 cm rocket tubes replaces the quad mount.
- **D Searchlight truck:** a tall searchlight mast on the bed, with a small flak mount behind.
- **E Armoured cab:** a full armoured cab with a slit windscreen and a crew shield over the mount.
- **F Flak 36 3.7 cm:** the quad mount is replaced by one long single barrel on a pedestal.

### Panzerjäger I
- **A Marder 4.7 ammo:** the existing gun with a visible tungsten sabot cradle.
- **B Ambush shield:** an angled gun shield and camouflage netting over the hull.
- **C Light tank hull:** the casemate is removed and replaced by a small turret with a 2 cm gun.
- **D Engineer dozer:** a dozer blade on the front and a mine-clearing roller.
- **E Sturmgeschütz I:** a stubby 7.5 cm howitzer with a fixed muzzle brake.
- **F Mobile battery:** a larger ammunition rack on the rear deck and two spare shells on the hull.

### Pz I 150 mm (Bison)
- **A Smoke battery:** smoke tubes bolted to the sides of the superstructure.
- **B Long tube:** a long, thin barrel with a slotted muzzle brake.
- **C Bison flamer:** the howitzer is replaced by a large flamethrower nozzle on the turret.
- **D Smoke launcher:** the howitzer is replaced by a multi-tube smoke launcher.
- **E Armoured shield:** a thick front shield around the howitzer.
- **F Direct fire:** the howitzer is replaced by a short high-velocity gun with a flat shield.

### Pz IV F1
- **A F1 assault:** a hull flamethrower nozzle next to a short-barrelled gun.
- **B Fast F1:** a larger engine grille and exhaust stack on the rear deck.
- **C F1 bridgelayer:** the turret is removed for a folding bridge on the rear deck.
- **D F1 recovery:** a crane and winch on the turret ring, with a tow bar at the rear.
- **E F2 conversion:** a long 7.5 cm barrel with a narrow muzzle brake.
- **F Command F1:** a larger cupola with a command periscope and a raised aerial.

### Pz 38(t) Railgun
- **A Capacitor armour:** boxy capacitor panels around the hull, with a visible glow.
- **B Sabot rail:** a long, thin rail barrel with a bulky counterweight at the rear.
- **C 38(t) mine layer:** the rail gun is replaced by a mine-dispenser rack on the rear deck.
- **D 38(t) sniper:** a long, narrow rifle barrel with a telescopic sight on the turret.
- **E Overcharge:** a broad, heavy capacitor bank replaces the rear deck.
- **F Spotter link:** a radio mast and a larger observation cupola.

---

## Medium tank factory

### Marder
- **A Marder III H:** a flat armoured roof with a thicker front plate.
- **B Infantry gun:** a short 15 cm infantry gun in place of the anti-tank gun.
- **C Marder recon:** the casemate is replaced by an open turret with a radio mast.
- **D Marder flame:** the gun is replaced by a flamethrower on the hull front.
- **E Tow and fight:** a PaK 40 hitch at the rear with the gun on a tow arm.
- **F Hit-and-run:** lightened armour with slatted side panels and a larger engine deck.

### Pz IV G
- **A G with Schürzen:** side skirts on the running gear.
- **B G long-barrel:** a long 7.5 cm barrel with a long muzzle brake.
- **C G carrier:** the turret is removed and replaced by a covered troop compartment.
- **D G mortar:** the turret is replaced by a 12 cm mortar on a pedestal.
- **E G assault:** a hull flamethrower and a short howitzer on the turret.
- **F G command:** a large cupola with a command aerial and a shortened gun.

### Jagdpanzer IV/70
- **A Jagdpanzer IV L/48:** a shorter, lighter gun with a smaller mantlet.
- **B Heavy mantlet:** a wide mantlet with a thick armoured front plate.
- **C Jagdpanzer IV bunker:** a fully enclosed casemate with firing ports, no visible gun.
- **D Jagdpanzer IV rocket:** the gun is replaced by a rocket launcher on the casemate roof.
- **E Ambush:** camouflage netting and a low gun shield.
- **F Lang ammo:** a long-barrelled gun with a large fume extractor.

### Brummbär
- **A Late Brummbär:** a cupola MG on the roof and side skirts.
- **B Armoured:** a heavy front plate and a slab-sided superstructure.
- **C Brummbär flamer:** the howitzer is replaced by a flamethrower nozzle on the turret.
- **D Brummbär tank hunter:** the howitzer is replaced by a long 7.5 cm anti-tank gun.
- **E Direct siege:** a shorter, squat gun with a wide breech.
- **F Wrecker:** a heavy spiked ram on the front of the hull.

### Pz IV H
- **A H standard plus:** Schürzen and a cupola gunner.
- **B H AT:** a long 7.5 cm gun with an APCR-style muzzle brake.
- **C H bridgelayer:** the turret is removed and replaced by a folding bridge.
- **D H rocket carrier:** the turret is replaced by a rocket rack on the rear deck.
- **E H HE:** a short, stubby gun replacing the long barrel.
- **F H fast:** a tuned engine with a large radiator grille and a rear exhaust stack.

### Pz IV rockets (Sturmpanzer / Panzerwerfer style)
- **A Incendiary salvo:** a rack of rocket tubes on the turret, with flame markings.
- **B Smoke salvo:** rocket tubes with smoke canisters beside them.
- **C Panzerwerfer flat-bed:** a wide flat bed of rocket tubes on the hull, with no turret.
- **D Rocket mortar:** a tall rocket launcher on a turntable with a small shield.
- **E Long salvo:** a longer, narrower rocket rack.
- **F Armoured launcher:** a boxed rocket housing with armour plates.

---

## Heavy and experimental factory

### E25
- **A Hunter:** camouflage netting over the hull.
- **B Armoured E25:** a thicker, flatter front plate with extra bolted armour.
- **C E25 bunker:** a fully enclosed casemate with firing ports and no turret.
- **D E25 flamer:** the main gun is replaced by a large flamethrower on the turret.
- **E HE E25:** a short, stubby assault gun.
- **F Spotter E25:** a raised observation cupola with a radio mast.

### Tiger H1
- **A Tiger E:** a thicker turret with a boxy front and extra armour.
- **B Tiger tow:** a tow hitch at the rear with an 8.8 cm gun trailing on it.
- **C Tiger artillery:** the turret is removed and replaced by a 10.5 cm howitzer on the hull.
- **D Tiger bridgelayer:** a folding bridge on the rear deck, no turret.
- **E Tiger HE:** a short, stubby gun on a wider mantlet.
- **F Tiger command:** a large cupola and command aerial, with a shorter gun.

### Tiger Rail
- **A Capacitor shield:** boxy capacitor panels around the hull, with a glow.
- **B Sabot rail:** a long rail barrel with a counterweight at the rear.
- **C Tiger Rail bunker:** a fixed armoured casemate with a rail gun and no turret.
- **D Tiger Rail mortar:** the rail gun is replaced by a 12 cm mortar on the turret.
- **E Overcharge:** a broad, heavy capacitor bank on the rear deck.
- **F Spotter link:** a radio mast and a larger observation cupola.

### SturmTiger
- **A Incendiary rocket:** a rocket launcher replaces the 38 cm mortar.
- **B Bunker buster:** a shorter, fat barrel with a heavy casing.
- **C SturmTiger flamer:** the mortar is replaced by a large flamethrower.
- **D SturmTiger carrier:** the turret is removed and replaced by a covered troop bay.
- **E Smoke rocket:** a rocket rack with smoke canisters.
- **F Double reload crew:** a second loader's hatch and an extra ammunition rack.

### Tiger II
- **A Porsche turret:** a smaller, rounded turret with a narrower gun.
- **B Henschel plus:** a wide, boxy turret with heavy front plates.
- **C Tiger II recovery:** the turret is removed and replaced by a winch and crane.
- **D Tiger II flamer:** the gun is replaced by a flamethrower on the turret.
- **E Tiger II HE:** a short, stubby gun with a wide mantlet.
- **F Tiger II command:** a large cupola with a command aerial.

### Tiger II 105
- **A Long 105:** a long 10.5 cm barrel with a long muzzle brake.
- **B HEAT 105:** a narrow, shorter 10.5 cm gun with a slim mantlet.
- **C Tiger II 105 bunker:** a fixed casemate with a 10.5 cm howitzer, no turret.
- **D Tiger II 105 mortar:** the howitzer is replaced by a 12 cm mortar on the turret.
- **E Siege 105:** a stubby howitzer with a wide muzzle.
- **F Twin loader:** a second loader's hatch and an extra ammunition bin.

### Mörser
- **A Incendiary:** a fire-marked launcher with rocket tubes.
- **B Smoke:** a tube launcher with smoke canisters on the sides.
- **C Mörser flamer:** the mortar is replaced by a flamethrower on the chassis.
- **D Mörser AA:** the mortar is replaced by an autocannon on a raised mount.
- **E Long range:** a longer, thinner tube with a bipod.
- **F Armoured mortar:** an armoured housing over the mortar tube.

### Maus
- **A Maus flak:** a 20 mm AA mount on the turret roof.
- **B Maus engine:** a larger engine deck and exhaust stacks.
- **C Maus bridgelayer:** the turret is removed and replaced by a folding bridge.
- **D Maus rocket:** the turret is replaced by a rocket rack on the hull.
- **E Maus siege:** a short, stubby gun with a wide breech.
- **F Maus fortress:** a dug-in hull with sandbags around it.

---

## Strike mechanized depot

### Sdkfz 251 transport
- **A Assault 251:** a second MG mount and extra armour plates on the hull sides.
- **B Medic 251:** a red-cross marking and a canvas canopy over the troop bay.
- **C 251 flame halftrack:** the open troop bay is replaced by a flamethrower on a pedestal.
- **D 251 recovery:** a crane and winch on the rear deck, no troop bay.
- **E Command 251:** a tall aerial and a shielded command hatch.
- **F Ammo 251:** a large ammunition crate on the rear deck.

### Sdkfz 140 (30 mm auto)
- **A AA 140:** an autocannon pointed skyward, on a raised mount.
- **B Armoured 140:** a heavier, boxier shield with extra plates.
- **C 140 bunker:** a fixed armoured casemate with the autocannon and no turret.
- **D 140 flamer:** the autocannon is replaced by a flamethrower.
- **E AP belt:** a longer barrel with a wide muzzle brake.
- **F Scout 140:** a tall aerial and a thin, low-profile hull.

### Sdkfz 221 20 mm
- **A Scout 221:** a tall aerial and an open armoured cab.
- **B Raider 221:** a sandbag ring around the open cab.
- **C 221 mortar:** a 5 cm mortar on a small turret, replacing the 2 cm gun.
- **D 221 recon radio:** a tall radio mast and an open armoured cab, no gun.
- **E 221 AA:** a twin 2 cm mount on a raised platform.
- **F 221 armoured:** a sheet-steel hull with a thick front shield.

### Sdkfz 251 mortar
- **A Smoke mortar:** smoke canisters on the rear rack.
- **B Long tube:** a longer, thinner mortar tube.
- **C Mortar carrier:** a 12 cm mortar on a pedestal replaces the rear troop bay.
- **D Rocket halftrack:** a rack of rocket tubes replaces the mortar.
- **E Rapid mortar:** a short, wide mortar with a double tube.
- **F Spotter mortar:** a raised observation cupola with a radio mast.

### Sdkfz 251/22 75 mm
- **A APCR:** a long anti-tank gun with a muzzle brake.
- **B HE:** a short, stubby howitzer.
- **C 251/22 flamer:** the gun is replaced by a flamethrower.
- **D 251/22 bridgelayer:** a folding bridge on the rear deck, no gun.
- **E Hull-down kit:** a sandbag ring and a dug-in pit.
- **F Double crew:** a second crew hatch and an extra ammunition rack.

---

## Light tank factory

### Pz II F
- **A Recon:** a longer antenna and a wide-view periscope cluster.
- **B Spaced armour:** side skirts and a roof grille.
- **C Pz II F recovery:** the turret is removed and replaced by a winch and crane.
- **D Pz II F mortar:** the turret is replaced by a small mortar on a pedestal.
- **E 5 cm gun:** a longer barrel with a slim muzzle brake.
- **F Flame II:** a flamethrower on the hull front, replacing the gun.

### Puma
- **A Puma 5 cm:** a long anti-tank gun with a slim mantlet.
- **B Puma recon:** a larger observation cupola and a radio mast.
- **C Puma flamer:** the gun is replaced by a flamethrower on the turret.
- **D Puma rocket:** the gun is replaced by a rocket rack on the turret.
- **E Puma 7.5 cm:** a stubby howitzer on the Puma chassis.
- **F Puma raider:** lighter armour, a visible engine grille and a rear exhaust stack.

### Panzerjäger I laser
- **A Overcharge:** a boxy laser emitter with a large glowing lens.
- **B Cutting beam:** a wide emitter with a flat, broad lens.
- **C Laser flamer:** the laser is replaced by a flamethrower nozzle.
- **D Laser turretless:** the turret is removed and replaced by a fixed casemate with a laser emitter.
- **E Rapid pulse:** a double emitter with twin lenses.
- **F Capacitor shield:** capacitor panels around the hull, with a glow.

### Sdkfz 234/3
- **A HE 234:** a short, stubby howitzer on a squat turret.
- **B Smoke 234:** smoke canisters on the turret sides.
- **C 234/3 bunker:** a fixed casemate with a 7.5 cm gun and no turret.
- **D 234/3 flamer:** the gun is replaced by a flamethrower on the turret.
- **E 234/2 Puma conversion:** an anti-tank turret with a long gun, no howitzer.
- **F Raider 234:** a lower hull with a large engine grille.

### Pz III J
- **A J long 5 cm:** a long anti-tank gun with a muzzle brake.
- **B J HE:** a short, stubby howitzer.
- **C Pz III J bridgelayer:** the turret is removed and replaced by a folding bridge.
- **D Pz III J mortar:** the turret is replaced by a 12 cm mortar.
- **E J Schürzen:** side skirts on the running gear.
- **F J command:** a large cupola with a command aerial.

### Pz III AA
- **A Twin 2 cm:** a twin AA mount on the turret.
- **B 3.7 cm single:** a single long-barrelled AA gun.
- **C AA flamer:** the twin AA guns are replaced by a flamethrower.
- **D AA bunker:** a fixed casemate with twin AA guns and no turret.
- **E Ground sweep:** a wide, low ring of infantry guns.
- **F Armoured AA:** a boxy armoured turret with sloped plates.

### Pz 38(t) rockets
- **A Incendiary:** a rack of rocket tubes with flame markings.
- **B Smoke:** rocket tubes with smoke canisters.
- **C 38(t) bridgelayer:** the rocket rack is removed and replaced by a folding bridge.
- **D 38(t) flamer:** the rocket rack is replaced by a flamethrower.
- **E Long rocket:** a longer, narrower rocket rack.
- **F Armoured rocket:** a boxed rocket housing with armour plates.

---

## Medium tank factory (Strike)

### StuG III
- **A StuG G:** a sloped front plate with extra armour.
- **B StuG ambush:** camouflage netting and a low gun shield.
- **C StuG recovery:** the casemate is replaced by a winch and crane.
- **D StuG flamer:** the gun is replaced by a flamethrower.
- **E StuH 42:** a 10.5 cm howitzer in a larger casemate.
- **F StuG long:** a long 7.5 cm barrel with a long muzzle brake.

### Jaguar 35 mm autocannon
- **A AA Jaguar:** a skyward autocannon mount.
- **B AP belt:** a long, slim autocannon with a wide muzzle brake.
- **C Jaguar flamer:** the autocannon is replaced by a flamethrower.
- **D Jaguar mortar:** the autocannon is replaced by a 12 cm mortar.
- **E HE belt:** a short, stubby mortar on the hull.
- **F Armoured Jaguar:** a sloped armoured superstructure.

### Hetzer
- **A Hetzer Starr:** a long barrel with a narrow muzzle brake.
- **B Hetzer ambush:** camouflage netting and a low gun shield.
- **C Hetzer recovery:** the casemate is replaced by a winch and crane.
- **D Hetzer rocket:** the gun is replaced by a rocket rack.
- **E Hetzer HE:** a short, stubby howitzer on a wider mantlet.
- **F Flammhetzer:** a flamethrower nozzle replaces the gun.

### Wespe
- **A Incendiary:** a rocket rack with flame markings.
- **B Smoke:** a rocket rack with smoke canisters.
- **C Wespe flamer:** the howitzer is replaced by a flamethrower.
- **D Wespe bunker:** a fixed casemate with a howitzer and no turret.
- **E Long rocket:** a longer, narrower rocket rack.
- **F Armoured Wespe:** an armoured shield around the howitzer.

### Pz III M
- **A M Schürzen:** side skirts on the running gear.
- **B M APCR:** a long anti-tank gun with a muzzle brake.
- **C Pz III M bridgelayer:** the turret is removed and replaced by a folding bridge.
- **D Pz III M recovery:** the turret is replaced by a crane and winch.
- **E M HE:** a short, stubby howitzer.
- **F M command:** a large cupola with a command aerial.

### Pz III M Flammpanzer
- **A Long flame:** a long flamethrower barrel.
- **B Napalm:** a wider nozzle with a fuel tank on the rear.
- **C Flamm carrier:** the turret is removed and replaced by a covered troop bay.
- **D Flamm mortar:** the flamethrower is replaced by a 12 cm mortar.
- **E Twin flame:** flamethrowers on the hull and turret.
- **F Smoke flame:** a flamethrower with a smoke launcher.

---

## Heavy tank factory (Strike)

### Panther D
- **A Panther A:** a thicker front plate with extra armour.
- **B Panther HE:** a short, stubby gun on a wider mantlet.
- **C Panther D bridgelayer:** the turret is removed and replaced by a folding bridge.
- **D Panther D recovery:** the turret is replaced by a crane and winch.
- **E Panther command:** a large cupola with a command aerial.
- **F Panther hunter:** camouflage netting and a low gun shield.

### Jagdpanther
- **A Ambush:** camouflage netting and a low gun shield.
- **B Heavy mantlet:** a wide mantlet with a thick armoured front plate.
- **C Jagdpanther flamer:** the gun is replaced by a flamethrower.
- **D Jagdpanther rocket:** the gun is replaced by a rocket rack.
- **E Long ammo:** a long-barrelled gun with a large fume extractor.
- **F Zimmerit crew:** a textured anti-magnetic coating on the hull.

### Pz IV/III laser
- **A Overcharge:** a boxy laser emitter with a glowing lens.
- **B Cutting beam:** a wide emitter with a flat lens.
- **C Laser recovery:** the turret is removed and replaced by a winch and crane.
- **D Laser flamer:** the laser is replaced by a flamethrower nozzle.
- **E Rapid pulse:** a double emitter with twin lenses.
- **F Capacitor shield:** capacitor panels around the hull, with a glow.

### Panther III
- **A Panther III APCR:** a long gun with a muzzle brake.
- **B Panther III HE:** a short, stubby gun.
- **C Panther III bridgelayer:** the turret is removed and replaced by a folding bridge.
- **D Panther III mortar:** the gun is replaced by a 12 cm mortar.
- **E Energy coupling:** a glowing capacitor plate on the hull.
- **F Armoured Panther III:** a thicker, flatter front plate.

### E100
- **A E100 flak:** a 20 mm AA mount on the turret.
- **B E100 siege:** a short, stubby gun with a wide breech.
- **C E100 bunker:** a fixed casemate with a 15 cm gun and no turret.
- **D E100 rocket:** the turret is replaced by a rocket rack.
- **E E100 command:** a large cupola with a command aerial.
- **F E100 fortress:** a dug-in hull with sandbags.

---

## Implementation notes

- The cap is 3 in `UnitVariants::MaxVariantsPerUnit`. Base plus six variants needs `VariantC` through `VariantF` appended to `EUnitVariant` (append-only), and the cap raised to 7.
- Radical variants (C, D) replace the turret or casemate, so they need their own mesh, skeleton and socket setup. Budget that separately from the subtle ones.
- Shared families (rockets, lasers, flamethrowers, artillery) should reuse one mesh set per family, so the player learns the look once.
