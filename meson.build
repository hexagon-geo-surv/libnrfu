project('libnrfu', 'c',
	 version : '1.0.0',
	 license : 'BSD-3-Clause')

inc = include_directories('include')

subdir('include')
subdir('lib')
subdir('tools')

if get_option('with-pymod')
	subdir('bindings/python')
endif
