py3_mod = import('python3')
py3 = py3_mod.find_python()
py3_dep = dependency('python3')

if get_option('python_site_dir') == ''
	python_dir = get_option('libdir')
else
	python_dir = get_option('python_site_dir')
endif

if py3_dep.found()
	pylib = py3_mod.extension_module('nrfu',
					 'nrfumodule.c',
					 dependencies : py3_dep,
					 include_directories : inc,
					 link_with : libnrfu,
					 install : true,
					 install_dir : python_dir
	)
else
	error('Python3 libraries not found')
endif
