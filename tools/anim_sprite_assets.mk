# Battle animation sprite tile data generated from the checked-in PNG sources.
# The target list is derived from the INCBIN references so it cannot drift.
GFX ?= tools/gbagfx/gbagfx

ANIM_SPRITE_DIR := graphics/battle_anims/sprites
ANIM_SPRITE_TARGETS := $(shell grep -oE 'graphics/battle_anims/sprites/[^"]+\.4bpp\.lz' src/graphics.c | sort -u)
ANIM_SPRITE_SPLIT_NAMES := flower ice_crystals ice_cube mud_sand spark
ANIM_SPRITE_SPLIT_4BPP := $(ANIM_SPRITE_SPLIT_NAMES:%=$(ANIM_SPRITE_DIR)/%.4bpp)
ANIM_SPRITE_4BPP := $(ANIM_SPRITE_TARGETS:.4bpp.lz=.4bpp)

# Keep the uncompressed files so a second invocation only rebuilds the five
# deliberately unconditional split sheets.
.SECONDARY: $(ANIM_SPRITE_4BPP)

.PHONY: anim-sprite-assets $(ANIM_SPRITE_SPLIT_4BPP)
anim-sprite-assets: $(ANIM_SPRITE_TARGETS)
	@for asset in $(ANIM_SPRITE_TARGETS); do test -s "$$asset" || exit 1; done

# Battle animation sheets are already arranged in the engine's tile order.
$(ANIM_SPRITE_DIR)/%.4bpp: $(ANIM_SPRITE_DIR)/%.png
	@mkdir -p $(dir $@)
	@$(GFX) $< $@

$(ANIM_SPRITE_DIR)/%.4bpp.lz: $(ANIM_SPRITE_DIR)/%.4bpp
	@mkdir -p $(dir $@)
	@$(GFX) $< $@

# These five source families are split across PNGs with different widths.
# Combine them vertically on a 64-pixel-wide indexed PNG before conversion.
ANIM_COMBINE_SCRIPT := exec("import sys,struct,zlib,binascii\n\ndef chunks(path):\n    data=open(path,\"rb\").read()\n    assert data[:8]==bytes.fromhex(\"89504e470d0a1a0a\")\n    pos=8\n    out=[]\n    while pos<len(data):\n        size=struct.unpack(\">I\",data[pos:pos+4])[0]\n        kind=data[pos+4:pos+8]\n        out.append((kind,data[pos+8:pos+8+size]))\n        pos += size+12\n    return out\n\ndef read_png(path):\n    cs=chunks(path)\n    ihdr=next(data for kind,data in cs if kind==b\"IHDR\")\n    width,height,depth,color,compression,filter_method,interlace=struct.unpack(\">IIBBBBB\",ihdr)\n    assert depth==4 and color==3 and compression==0 and filter_method==0 and interlace==0\n    palette=next(data for kind,data in cs if kind==b\"PLTE\")\n    packed_row=(width+1)//2\n    raw=zlib.decompress(b\"\".join(data for kind,data in cs if kind==b\"IDAT\"))\n    rows=[]\n    pos=0\n    previous=bytearray(packed_row)\n    for row_index in range(height):\n        filter_type=raw[pos]\n        pos += 1\n        row=bytearray(raw[pos:pos+packed_row])\n        pos += packed_row\n        for index in range(packed_row):\n            left=row[index-1] if index else 0\n            above=previous[index]\n            upper_left=previous[index-1] if index else 0\n            if filter_type==1:\n                row[index]=(row[index]+left)&255\n            elif filter_type==2:\n                row[index]=(row[index]+above)&255\n            elif filter_type==3:\n                row[index]=(row[index]+((left+above)//2))&255\n            elif filter_type==4:\n                predictor=left+above-upper_left\n                left_distance=abs(predictor-left)\n                above_distance=abs(predictor-above)\n                upper_left_distance=abs(predictor-upper_left)\n                if left_distance<=above_distance and left_distance<=upper_left_distance:\n                    nearest=left\n                elif above_distance<=upper_left_distance:\n                    nearest=above\n                else:\n                    nearest=upper_left\n                row[index]=(row[index]+nearest)&255\n            elif filter_type!=0:\n                raise ValueError(filter_type)\n        rows.append(row)\n        previous=row\n    assert pos==len(raw)\n    return width,height,palette,rows\n\ndef chunk(kind,data):\n    return struct.pack(\">I\",len(data))+kind+data+struct.pack(\">I\",binascii.crc32(kind+data)&0xffffffff)\n\ninputs=sys.argv[2:]\nimages=[read_png(path) for path in inputs]\nassert images and all(depth[0]<=64 and depth[2]==images[0][2] for depth in images)\ntotal_height=sum(image[1] for image in images)\nout_rows=[]\nfor width,height,palette,rows in images:\n    for row in rows:\n        pixels=[]\n        for value in row:\n            pixels.extend((value>>4,value&15))\n        pixels=pixels[:width]+[0]*(64-width)\n        out_rows.append(bytes((pixels[index]<<4)|pixels[index+1] for index in range(0,64,2)))\nraw=b\"\".join(bytes([0])+row for row in out_rows)\nheader=struct.pack(\">IIBBBBB\",64,total_height,4,3,0,0,0)\noutput=bytes.fromhex(\"89504e470d0a1a0a\")+chunk(b\"IHDR\",header)+chunk(b\"PLTE\",images[0][2])+chunk(b\"IDAT\",zlib.compress(raw))+chunk(b\"IEND\",b\"\")\nopen(sys.argv[1],\"wb\").write(output)\n")

$(ANIM_SPRITE_DIR)/flower.4bpp: $(ANIM_SPRITE_DIR)/flower_0.png $(ANIM_SPRITE_DIR)/flower_1.png
	@mkdir -p $(dir $@)
	@trap 'rm -f "$@.png"' 0; python3 -c '$(ANIM_COMBINE_SCRIPT)' "$@.png" $^ && $(GFX) "$@.png" "$@"

$(ANIM_SPRITE_DIR)/ice_crystals.4bpp: $(ANIM_SPRITE_DIR)/ice_crystals_0.png $(ANIM_SPRITE_DIR)/ice_crystals_1.png $(ANIM_SPRITE_DIR)/ice_crystals_2.png $(ANIM_SPRITE_DIR)/ice_crystals_3.png $(ANIM_SPRITE_DIR)/ice_crystals_4.png
	@mkdir -p $(dir $@)
	@trap 'rm -f "$@.png"' 0; python3 -c '$(ANIM_COMBINE_SCRIPT)' "$@.png" $^ && $(GFX) "$@.png" "$@"

$(ANIM_SPRITE_DIR)/ice_cube.4bpp: $(ANIM_SPRITE_DIR)/ice_cube_0.png $(ANIM_SPRITE_DIR)/ice_cube_1.png $(ANIM_SPRITE_DIR)/ice_cube_2.png $(ANIM_SPRITE_DIR)/ice_cube_3.png
	@mkdir -p $(dir $@)
	@trap 'rm -f "$@.png"' 0; python3 -c '$(ANIM_COMBINE_SCRIPT)' "$@.png" $^ && $(GFX) "$@.png" "$@"

$(ANIM_SPRITE_DIR)/mud_sand.4bpp: $(ANIM_SPRITE_DIR)/mud_sand_0.png $(ANIM_SPRITE_DIR)/mud_sand_1.png
	@mkdir -p $(dir $@)
	@trap 'rm -f "$@.png"' 0; python3 -c '$(ANIM_COMBINE_SCRIPT)' "$@.png" $^ && $(GFX) "$@.png" "$@"

$(ANIM_SPRITE_DIR)/spark.4bpp: $(ANIM_SPRITE_DIR)/spark_0.png $(ANIM_SPRITE_DIR)/spark_1.png
	@mkdir -p $(dir $@)
	@trap 'rm -f "$@.png"' 0; python3 -c '$(ANIM_COMBINE_SCRIPT)' "$@.png" $^ && $(GFX) "$@.png" "$@"
