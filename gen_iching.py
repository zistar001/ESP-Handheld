import json

with open('out.txt', 'r', encoding='utf-8') as f:
    data = [json.loads(line) for line in f if line.strip()]

keys = ['name','guaci','xiang','daxiang','yushi','shiye','jingshang','qiuming','hunlian','juece']

with open('main/modules/iching/iching_data.c', 'w', encoding='utf-8') as src:
    src.write('#include "iching_data.h"\n\n')
    src.write('const iching_hexagram_t g_iching[64] = {\n')
    for d in data:
        src.write('    {')
        for k in keys:
            v = d.get(k, '')
            v = v.replace('\\', '\\\\').replace('"', '\\"')
            src.write('"' + v + '", ')
        src.write('},\n')
    src.write('};\n')
print('Done')
