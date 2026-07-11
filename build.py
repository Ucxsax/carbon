import subprocess, os

vcvarsall = r'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat'
project_dir = r'E:\Carbon\carbon'
build_dir = os.path.join(project_dir, 'build')

os.makedirs(build_dir, exist_ok=True)

include_dir = os.path.join(project_dir, 'include')
winsdk_base = r'D:\Program Files (x86)\Windows Kits\10\include\10.0.26100.0'
winsdk_um = os.path.join(winsdk_base, 'um')
winsdk_shared = os.path.join(winsdk_base, 'shared')

# Source files
sources = [
    r'src\utils\logger.cpp',
    r'src\scheduler\task_queue.cpp',
    r'src\hal\npu_backend.cpp',
    r'src\backends\mock_backend.cpp',
    r'src\backends\openvino_backend.cpp',
    r'src\backends\directml_backend.cpp',
    r'src\backends\tensorrt_backend.cpp',
    r'src\backends\ort_backend.cpp',
    r'src\compute\chunk_mesh_processor.cpp',
    r'src\compute\light_bake_processor.cpp',
    r'src\scheduler\npu_scheduler.cpp',
]

# Build the batch file content
lines = [
    '@echo off',
    f'call "{vcvarsall}" x64',
    'echo === Setting up environment ===',
    'echo cl.exe:',
    'where cl.exe',
    'echo === Starting compilation ===',
]

# Compile each file individually
for src in sources:
    src_path = os.path.join(project_dir, src)
    basename = os.path.basename(src).replace('.cpp', '.obj')
    obj_path = os.path.join(build_dir, basename)
    lines.append(f'echo Compiling {src}...')
    lines.append(f'cl.exe /std:c++20 /EHsc /W4 /utf-8 /D CARBON_PLATFORM_WINDOWS /I "{include_dir}" /I "{winsdk_um}" /I "{winsdk_shared}" /c "{src_path}" /Fo:"{obj_path}" 2>&1')
    lines.append('if %errorlevel% neq 0 echo FAILED: ' + src)
    lines.append('')

# Create library
obj_files = ' '.join([os.path.join(build_dir, os.path.basename(s).replace('.cpp', '.obj')) for s in sources])
lines.append('echo === Creating library ===')
lines.append(f'lib.exe /OUT:"{os.path.join(build_dir, "carbon.lib")}" {obj_files} 2>&1')
lines.append('echo === Build complete ===')

batch_path = os.path.join(os.environ['TEMP'], 'build_carbon_v2.bat')
with open(batch_path, 'w', encoding='ascii') as f:
    f.write('\n'.join(lines))

print(f'Batch file: {batch_path}')
print(f'Compiling {len(sources)} files...')

result = subprocess.run(
    ['cmd', '/c', batch_path],
    capture_output=True, text=True, timeout=300
)
print(result.stdout)
if result.returncode != 0:
    print(f'Exit code: {result.returncode}')
if result.stderr:
    print(f'STDERR: {result.stderr[:3000]}')
