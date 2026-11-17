"""Make report paths portable before committing generated evidence."""
import pathlib,re
root=pathlib.Path(__file__).resolve().parents[1]
paths={str(root),root.as_posix()}
if root.as_posix().startswith('/mnt/'):
    parts=root.as_posix().split('/')
    windows=parts[2].upper()+':/'+'/'.join(parts[3:])
    paths.update({windows,windows.replace('/','\\'),windows.replace('/','\\\\')})
for file in (root/'docs/reports').rglob('*'):
    if not file.is_file() or file.suffix not in {'.log','.xml','.json','.html','.txt','.css'}: continue
    text=file.read_text(encoding='utf-8-sig')
    for path in sorted(paths,key=len,reverse=True): text=re.sub(re.escape(path),'.',text,flags=re.I)
    file.write_text(text,encoding='utf-8')
print('Normalized report paths for portable repository evidence')
