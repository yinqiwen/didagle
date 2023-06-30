from setuptools import setup

setup(
    name='pydidagle',
    version='0.1.0',
    description='A example Python package',
    url='https://git.woa.com/lm-engine/pydidagle.git',
    author='yinqiwen',
    author_email='yinqiwen@gmail.com',
    license='BSD 2-clause',
    packages=['pydidagle'],
    install_requires=['dacite',
                      'toml',
                      'graphviz',
                      'dataclasses-json',
                      ],

    # classifiers=[
    #     'Development Status :: 1 - Planning',
    #     'Intended Audience :: Science/Research',
    #     'License :: OSI Approved :: BSD License',
    #     'Operating System :: POSIX :: Linux',
    #     'Programming Language :: Python :: 2',
    #     'Programming Language :: Python :: 2.7',
    #     'Programming Language :: Python :: 3',
    #     'Programming Language :: Python :: 3.4',
    #     'Programming Language :: Python :: 3.5',
    # ],
)
