from setuptools import setup, find_packages
setup(
    name="qubesfiletrust",
    version="0.1.0",
    packages=find_packages(),
    entry_points={
        "console_scripts": [
            "qvm-file-trust = qubesfiletrust.qvm_file_trust:main",
        ]
    },

    # Module dependencies
    install_requires=["xattr>=0.9.1"],

    # Metadata
    author="Andrew Morgan",
    author_email="andrew@amorgan.xyz",
    description="cli client for viewing and modifying file trust levels for QubesOS",
    keywords="qubes qvm trust",
)
