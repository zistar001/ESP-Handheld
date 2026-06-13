import re

with open('YI64.md', 'r', encoding='utf-8') as f:
    text = f.read()

entries = re.split(r'第\d+卦 ', text)[1:]
result = []
for e in entries:
    lines = e.strip().split('\n')
    name = lines[0].strip()
    gua_ci = ''
    xiang = ''
    daxiang = ''
    yushi = shiye = jingshang = qiuming = hunlian = juece = ''
    current_text = []
    
    for line in lines[1:]:
        line = line.strip()
        if not line:
            continue
        if line.startswith('《象》曰：'):
            if current_text:
                gua_ci = ' '.join(current_text)
                current_text = []
            xiang = line[5:]
        elif line.startswith('大象：'):
            if current_text and not gua_ci:
                gua_ci = ' '.join(current_text)
                current_text = []
            daxiang = line[3:]
        elif line.startswith('运势：'):
            yushi = line[3:]
        elif line.startswith('事业：'):
            shiye = line[3:]
        elif line.startswith('经商：'):
            jingshang = line[3:]
        elif line.startswith('求名：'):
            qiuming = line[3:]
        elif line.startswith('婚恋：'):
            hunlian = line[3:]
        elif line.startswith('决策：'):
            juece = line[3:]
        elif not gua_ci and not daxiang:
            current_text.append(line)
    
    if current_text and not gua_ci:
        gua_ci = ' '.join(current_text)
    
    result.append({
        'name': name, 'gua_ci': gua_ci, 'xiang': xiang,
        'daxiang': daxiang, 'yushi': yushi, 'shiye': shiye,
        'jingshang': jingshang, 'qiuming': qiuming,
        'hunlian': hunlian, 'juece': juece
    })

print(f'Total: {len(result)} hexagrams')
for i, r in enumerate(result):
    print(f'{i}: {r["name"]}')
