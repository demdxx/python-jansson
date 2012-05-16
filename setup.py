from distutils.core import setup, Extension

__version__ = "0.0.0"

module = Extension('pyjansson',
    define_macros = [('MAJOR_VERSION', '0'),
                     ('MINOR_VERSION', '0'),
                     ('MODULE_VERSION', '"%s"' % __version__)],
    include_dirs = ['/usr/local/include',
                    '/opt/local/include'],
    libraries = ['jansson'],
    library_dirs = ['/usr/local/lib'],
    sources = ['src/pyjansson.c']
)

setup(
    name = 'pyjansson',
    version = '0.0',
    description = 'Python bind for jansson',
    author = 'Dmitriy Ponomarev',
    author_email = 'demdxx@gmail.com',
    url = 'http://github.com/demdxx/python-jansson',
    long_description = open('README.rst').read(),
    ext_modules = [module],
    license='MIT',

    classifiers=[
        'Development Status :: 4 - Beta',
        'License :: OSI Approved :: MIT License',
        'Operating System :: OS Independent',
        'Programming Language :: C',

        'Intended Audience :: Developers',
        'Topic :: Software Development :: Libraries :: Python Modules',
    ]
)