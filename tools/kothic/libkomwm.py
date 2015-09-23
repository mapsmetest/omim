from drules_struct_pb2 import *
from timer import *
from mapcss import MapCSS
from optparse import OptionParser
import os
import csv
import sys
import json
import mapcss.webcolors
whatever_to_hex = mapcss.webcolors.webcolors.whatever_to_hex
whatever_to_cairo = mapcss.webcolors.webcolors.whatever_to_cairo

WIDTH_SCALE = 1.0

def komap_mapswithme(options, style, filename):
    if options.outfile == "-":
        print "Please specify base output path."
        exit()
    else:
        ddir = os.path.dirname(options.outfile)

    types_file = open(os.path.join(ddir, 'types.txt'), "w")
    drules_bin = open(os.path.join(options.outfile + '.bin'), "wb")
    drules_txt = open(os.path.join(options.outfile + '.txt'), "wb")

    drules = ContainerProto()
    classificator = {}
    class_order = []
    class_tree = {}
    visibility = {}
    textures = {}

    colors_file_name = os.path.join(ddir, 'colors.txt')
    colors = set()
    if os.path.exists(colors_file_name):
        colors_in_file = open(colors_file_name, "r")
        for colorLine in colors_in_file:
            colors.add(int(colorLine))
        colors_in_file.close()
    colors_file = open(colors_file_name, "w")

    patterns = []
    def addPattern(dashes):
        if dashes:
            for pattern in patterns:
                if (pattern == dashes):
                    return
            patterns.append(dashes)

    patterns_file_name = os.path.join(ddir, 'patterns.txt')
    if os.path.exists(patterns_file_name):
        patterns_in_file = open(patterns_file_name, "r")
        for patternsLine in patterns_in_file:
            addPattern([float(x) for x in patternsLine.split()])
        patterns_in_file.close()
    patterns_file = open(patterns_file_name, "w")

    for row in csv.reader(open(os.path.join(ddir, 'mapcss-mapping.csv')), delimiter=';'):
        pairs = [i.strip(']').split("=") for i in row[1].split(',')[0].split('[')]
        kv = {}
        for i in pairs:
            if len(i) == 1:
                if i[0]:
                    if i[0][0] == "!":
                        kv[i[0][1:].strip('?')] = "no"
                    else:
                        kv[i[0].strip('?')] = "yes"
            else:
                kv[i[0]] = i[1]
        classificator[row[0].replace("|", "-")] = kv
        if row[2] != "x":
            class_order.append(row[0].replace("|", "-"))
            print >> types_file, row[0]
        else:
            # compatibility mode
            if row[6]:
                print >> types_file, row[6]
            else:
                print >> types_file, "mapswithme"
        class_tree[row[0].replace("|", "-")] = row[0]
    class_order.sort()
    types_file.close()

    def mwm_encode_color(st, prefix='', default='black'):
        if prefix:
            prefix += "-"
        opacity = hex(255 - int(255 * float(st.get(prefix + "opacity", 1))))
        color = whatever_to_hex(st.get(prefix + 'color', default))
        color = color[1] + color[1] + color[3] + color[3] + color[5] + color[5]
        result = int(opacity + color, 16)
        colors.add(result)
        return result

    def mwm_encode_image(st, prefix='icon', bgprefix='symbol'):
        if prefix:
            prefix += "-"
        if bgprefix:
            bgprefix += "-"
        if prefix + "image" not in st:
            return False
        # strip last ".svg"
        handle = st.get(prefix + "image")[:-4]
        return handle, handle

    bgpos = 0

    dr_linecaps = {'none': BUTTCAP, 'butt': BUTTCAP, 'round': ROUNDCAP}
    dr_linejoins = {'none': NOJOIN, 'bevel': BEVELJOIN, 'round': ROUNDJOIN}

    # atbuild = AccumulativeTimer()
    # atzstyles = AccumulativeTimer()
    # atdrcont = AccumulativeTimer()
    # atline = AccumulativeTimer()
    # atarea = AccumulativeTimer()
    # atnode = AccumulativeTimer()

    # atbuild.Start()

    for cl in class_order:
        clname = cl if cl.find('-') == -1 else cl[:cl.find('-')]
        # clname = cl
        style.build_choosers_tree(clname, "line", classificator[cl])
        style.build_choosers_tree(clname, "area", classificator[cl])
        style.build_choosers_tree(clname, "node", classificator[cl])

    style.restore_choosers_order("line");
    style.restore_choosers_order("area");
    style.restore_choosers_order("node");

    # atbuild.Stop()

    for cl in class_order:
        visstring = ["0"] * (options.maxzoom - options.minzoom + 1)

        clname = cl if cl.find('-') == -1 else cl[:cl.find('-')]
        # clname = cl
        txclass = classificator[cl]
        txclass["name"] = "name"
        txclass["addr:housenumber"] = "addr:housenumber"
        txclass["ref"] = "ref"
        txclass["int_name"] = "int_name"
        txclass["addr:flats"] = "addr:flats"

        prev_area_len = -1
        prev_node_len = -1
        prev_line_len = -1
        check_area = True
        check_node = True
        check_line = True

        # atzstyles.Start()

        zstyles_arr = [None] * (options.maxzoom - options.minzoom + 1)
        has_icons_for_areas_arr = [False] * (options.maxzoom - options.minzoom + 1)

        for zoom in xrange(options.maxzoom, options.minzoom - 1, -1):
            has_icons_for_areas = False
            zstyle = {}

            if check_line:
                if "area" not in txclass:
                    # atline.Start()
                    linestyle = style.get_style_dict(clname, "line", txclass, zoom, olddict=zstyle, cache=False)
                    if prev_line_len == -1:
                        prev_line_len = len(linestyle)
                    if len(linestyle) == 0:
                        if prev_line_len != 0:
                            check_line = False
                    zstyle = linestyle
                    # atline.Stop()

            if check_area:
                # atarea.Start()
                areastyle = style.get_style_dict(clname, "area", txclass, zoom, olddict=zstyle, cache=False)
                for st in areastyle.values():
                    if "icon-image" in st or 'symbol-shape' in st or 'symbol-image' in st:
                        has_icons_for_areas = True
                        break
                if prev_area_len == -1:
                    prev_area_len = len(areastyle)
                if len(areastyle) == 0:
                    if prev_area_len != 0:
                        check_area = False
                zstyle = areastyle
                # atarea.Stop()

            if check_node:
                if "area" not in txclass:
                    # atnode.Start()
                    nodestyle = style.get_style_dict(clname, "node", txclass, zoom, olddict=zstyle, cache=False)
                    if prev_node_len == -1:
                        prev_node_len = len(nodestyle)
                    if len(nodestyle) == 0:
                        if prev_node_len != 0:
                            check_node = False
                    zstyle = nodestyle
                    # atnode.Stop()

            if not check_line and not check_area and not check_node:
                break

            zstyle = zstyle.values()

            zstyles_arr[zoom - options.minzoom] = zstyle
            has_icons_for_areas_arr[zoom - options.minzoom]= has_icons_for_areas

        # atzstyles.Stop()

        # atdrcont.Start()

        dr_cont = ClassifElementProto()
        dr_cont.name = cl
        for zoom in xrange(options.minzoom, options.maxzoom + 1):
            zstyle = zstyles_arr[zoom - options.minzoom]
            if zstyle is None or len(zstyle) == 0:
                continue
            has_icons_for_areas = has_icons_for_areas_arr[zoom - options.minzoom]

            has_lines = False
            has_icons = False
            has_fills = False
            for st in zstyle:
                st = dict([(k, v) for k, v in st.iteritems() if str(v).strip(" 0.")])
                if 'width' in st or 'pattern-image' in st:
                    has_lines = True
                if 'icon-image' in st or 'symbol-shape' in st or 'symbol-image' in st:
                    has_icons = True
                if 'fill-color' in st:
                    has_fills = True

            has_text = None
            txfmt = []
            for st in zstyle:
                if st.get('text') and not st.get('text') in txfmt:
                    txfmt.append(st.get('text'))
                    if has_text is None:
                        has_text = []
                    has_text.append(st)

            if has_lines or has_text or has_fills or has_icons:
                visstring[zoom] = "1"
                dr_element = DrawElementProto()
                dr_element.scale = zoom

                for st in zstyle:
                    if st.get('-x-kot-layer') == 'top':
                        st['z-index'] = float(st.get('z-index', 0)) + 15001.
                    elif st.get('-x-kot-layer') == 'bottom':
                        st['z-index'] = float(st.get('z-index', 0)) - 15001.

                    if st.get('casing-width') not in (None, 0):  # and (st.get('width') or st.get('fill-color')):
                        if st.get('casing-linecap', 'butt') == 'butt':
                            dr_line = LineRuleProto()
                            dr_line.width = (st.get('width', 0) * WIDTH_SCALE) + (st.get('casing-width') * WIDTH_SCALE * 2)
                            dr_line.color = mwm_encode_color(st, "casing")
                            dr_line.priority = min(int(st.get('z-index', 0) + 999), 20000)
                            dashes = st.get('casing-dashes', st.get('dashes', []))
                            dr_line.dashdot.dd.extend(dashes)
                            addPattern(dr_line.dashdot.dd)
                            dr_line.cap = dr_linecaps.get(st.get('casing-linecap', 'butt'), BUTTCAP)
                            dr_line.join = dr_linejoins.get(st.get('casing-linejoin', 'round'), ROUNDJOIN)
                            dr_element.lines.extend([dr_line])

                        # Let's try without this additional line style overhead. Needed only for casing in road endings.
                        # if st.get('casing-linecap', st.get('linecap', 'round')) != 'butt':
                        #     dr_line = LineRuleProto()
                        #     dr_line.width = (st.get('width', 0) * WIDTH_SCALE) + (st.get('casing-width') * WIDTH_SCALE * 2)
                        #     dr_line.color = mwm_encode_color(st, "casing")
                        #     dr_line.priority = -15000
                        #     dashes = st.get('casing-dashes', st.get('dashes', []))
                        #     dr_line.dashdot.dd.extend(dashes)
                        #     dr_line.cap = dr_linecaps.get(st.get('casing-linecap', 'round'), ROUNDCAP)
                        #     dr_line.join = dr_linejoins.get(st.get('casing-linejoin', 'round'), ROUNDJOIN)
                        #     dr_element.lines.extend([dr_line])

                    if has_lines:
                        if st.get('width'):
                            dr_line = LineRuleProto()
                            dr_line.width = (st.get('width', 0) * WIDTH_SCALE)
                            dr_line.color = mwm_encode_color(st)
                            for i in st.get('dashes', []):
                                dr_line.dashdot.dd.extend([max(float(i), 1) * WIDTH_SCALE])
                            addPattern(dr_line.dashdot.dd)
                            dr_line.cap = dr_linecaps.get(st.get('linecap', 'butt'), BUTTCAP)
                            dr_line.join = dr_linejoins.get(st.get('linejoin', 'round'), ROUNDJOIN)
                            dr_line.priority = min((int(st.get('z-index', 0)) + 1000), 20000)
                            dr_element.lines.extend([dr_line])
                        if st.get('pattern-image'):
                            dr_line = LineRuleProto()
                            dr_line.width = 0
                            dr_line.color = 0
                            icon = mwm_encode_image(st, prefix='pattern')
                            dr_line.pathsym.name = icon[0]
                            dr_line.pathsym.step = float(st.get('pattern-spacing', 0)) - 16
                            dr_line.pathsym.offset = st.get('pattern-offset', 0)
                            dr_line.priority = int(st.get('z-index', 0)) + 1000
                            dr_element.lines.extend([dr_line])
                            textures[icon[0]] = icon[1]
                        if st.get('shield-font-size'):
                            dr_element.shield.height = int(st.get('shield-font-size', 10))
                            dr_element.shield.color = mwm_encode_color(st, "shield-text")
                            if st.get('shield-text-halo-radius', 0) != 0:
                                dr_element.shield.stroke_color = mwm_encode_color(st, "shield-text-halo", "white")
                            dr_element.shield.priority = min(19100, (16000 + int(st.get('z-index', 0))))

                    if has_icons:
                        if st.get('icon-image'):
                            if not has_icons_for_areas:
                                dr_element.symbol.apply_for_type = 1
                            icon = mwm_encode_image(st)
                            dr_element.symbol.name = icon[0]
                            dr_element.symbol.priority = min(19100, (16000 + int(st.get('z-index', 0))))
                            textures[icon[0]] = icon[1]
                            has_icons = False
                        if st.get('symbol-shape'):
                            dr_element.circle.radius = float(st.get('symbol-size'))
                            dr_element.circle.color = mwm_encode_color(st, 'symbol-fill')
                            dr_element.circle.priority = min(19000, (14000 + int(st.get('z-index', 0))))
                            has_icons = False

                    if has_text and st.get('text'):
                        has_text = has_text[:2]
                        has_text.reverse()
                        dr_text = dr_element.path_text
                        base_z = 15000
                        if st.get('text-position', 'center') == 'line':
                            dr_text = dr_element.path_text
                            base_z = 16000
                        else:
                            dr_text = dr_element.caption
                        for sp in has_text[:]:
                            dr_cur_subtext = dr_text.primary
                            if len(has_text) == 2:
                                dr_cur_subtext = dr_text.secondary
                            dr_cur_subtext.height = int(float(sp.get('font-size', "10").split(",")[0]))
                            dr_cur_subtext.color = mwm_encode_color(sp, "text")
                            if st.get('text-halo-radius', 0) != 0:
                                dr_cur_subtext.stroke_color = mwm_encode_color(sp, "text-halo", "white")
                            if 'text-offset' in sp or 'text-offset-y' in sp:
                                dr_cur_subtext.offset_y = int(sp.get('text-offset-y', sp.get('text-offset', 0)))
                            if 'text-offset-x' in sp:
                                dr_cur_subtext.offset_x = int(sp.get('text-offset-x', 0))
                            has_text.pop()
                        dr_text.priority = min(19000, (base_z + int(st.get('z-index', 0))))
                        has_text = None

                    if has_fills:
                        if ('fill-color' in st) and (float(st.get('fill-opacity', 1)) > 0):
                            dr_element.area.color = mwm_encode_color(st, "fill")
                            if st.get('fill-position', 'foreground') == 'background':
                                if 'z-index' not in st:
                                    bgpos -= 1
                                    dr_element.area.priority = bgpos - 16000
                                else:
                                    zzz = int(st.get('z-index', 0))
                                    if zzz > 0:
                                        dr_element.area.priority = zzz - 16000
                                    else:
                                        dr_element.area.priority = zzz - 16700
                            else:
                                dr_element.area.priority = (int(st.get('z-index', 0)) + 1 + 1000)
                            has_fills = False

                dr_cont.element.extend([dr_element])

        if dr_cont.element:
            drules.cont.extend([dr_cont])

        # atdrcont.Stop()

        visibility["world|" + class_tree[cl] + "|"] = "".join(visstring)

    # atwrite = AccumulativeTimer()
    # atwrite.Start()

    drules_bin.write(drules.SerializeToString())
    drules_txt.write(unicode(drules))

    visnodes = set()
    for k, v in visibility.iteritems():
        vis = k.split("|")
        for i in range(1, len(vis) - 1):
            visnodes.add("|".join(vis[0:i]) + "|")
    viskeys = list(set(visibility.keys() + list(visnodes)))

    def cmprepl(a, b):
        if a == b:
            return 0
        a = a.replace("|", "-")
        b = b.replace("|", "-")
        if a > b:
            return 1
        return -1
    viskeys.sort(cmprepl)

    visibility_file = open(os.path.join(ddir, 'visibility.txt'), "w")
    classificator_file = open(os.path.join(ddir, 'classificator.txt'), "w")

    oldoffset = ""
    for k in viskeys:
        offset = "    " * (k.count("|") - 1)
        for i in range(len(oldoffset) / 4, len(offset) / 4, -1):
            print >> visibility_file, "    " * i + "{}"
            print >> classificator_file, "    " * i + "{}"

        oldoffset = offset
        end = "-"
        if k in visnodes:
            end = "+"
        print >> visibility_file, offset + k.split("|")[-2] + "  " + visibility.get(k, "0" * (options.maxzoom + 1)) + "  " + end
        print >> classificator_file, offset + k.split("|")[-2] + "  " + end
    for i in range(len(offset) / 4, 0, -1):
        print >> visibility_file, "    " * i + "{}"
        print >> classificator_file, "    " * i + "{}"

    # atwrite.Stop()

    for c in sorted(colors):
        colors_file.write("%d\n" % (c))
    colors_file.close()

    for p in patterns:
        patterns_file.write("%s\n" % (' '.join(str(elem) for elem in p)))
    patterns_file.close()

    # print "build, sec: %s" % (atbuild.ElapsedSec())
    # print "zstyle %s times, sec: %s" % (atzstyles.Count(), atzstyles.ElapsedSec())
    # print "drcont %s times, sec: %s" % (atdrcont.Count(), atdrcont.ElapsedSec())
    # print "line %s times, sec: %s" % (atline.Count(), atline.ElapsedSec())
    # print "area %s times, sec: %s" % (atarea.Count(), atarea.ElapsedSec())
    # print "node %s times, sec: %s" % (atnode.Count(), atnode.ElapsedSec())
    # print "writing files, sec: %s" % (atwrite.ElapsedSec())

# Main

parser = OptionParser()
parser.add_option("-s", "--stylesheet", dest="filename",
                  help="read MapCSS stylesheet from FILE", metavar="FILE")
parser.add_option("-f", "--minzoom", dest="minzoom", default=0, type="int",
                  help="minimal available zoom level", metavar="ZOOM")
parser.add_option("-t", "--maxzoom", dest="maxzoom", default=19, type="int",
                  help="maximal available zoom level", metavar="ZOOM")
parser.add_option("-l", "--locale", dest="locale",
                  help="language that should be used for labels (ru, en, be, uk..)", metavar="LANG")
parser.add_option("-o", "--output-file", dest="outfile", default="-",
                  help="output filename (defaults to stdout)", metavar="FILE")
parser.add_option("-p", "--osm2pgsql-style", dest="osm2pgsqlstyle", default="-",
                  help="osm2pgsql stylesheet filename", metavar="FILE")
parser.add_option("-b", "--background-only", dest="bgonly", action="store_true", default=False,
                  help="Skip rendering of icons and labels", metavar="BOOL")
parser.add_option("-T", "--text-scale", dest="textscale", default=1, type="float",
                  help="text size scale", metavar="SCALE")
parser.add_option("-c", "--config", dest="conffile", default="komap.conf",
                  help="config file name", metavar="FILE")

(options, args) = parser.parse_args()

if (options.filename is None):
  parser.error("MapCSS stylesheet filename is required")

try:
    # atparse = AccumulativeTimer()
    # atbuild = AccumulativeTimer()

    # atparse.Start()
    style = MapCSS(options.minzoom, options.maxzoom + 1)  # zoom levels
    style.parse(filename = options.filename)
    # atparse.Stop()

    # atbuild.Start()
    komap_mapswithme(options, style, options.filename)
    # atbuild.Stop()

    # print "mapcss parse, sec: %s" % (atparse.ElapsedSec())
    # print "build, sec: %s" % (atbuild.ElapsedSec())

    exit(0)

except Exception as e:
    print >> sys.stderr, "Error\n" + str(e)
    exit(-1)
