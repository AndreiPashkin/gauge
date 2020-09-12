import dunamai


SEMVER_PATTERN = (
    r'^(?P<base>(?P<major>0|[1-9]\d*)\.(?P<minor>0|[1-9]\d*)\.'
    r'(?P<patch>0|[1-9]\d*))'
    r'(?:-(?P<stage>(?:0|[1-9]\d*|\d*[a-zA-Z-][0-9a-zA-Z-]*)'
    r'(?:\.(?:0|[1-9]\d*|\d*[a-zA-Z-][0-9a-zA-Z-]*))*))?'
    r'(?:\+(?P<revision>[0-9a-zA-Z-]+(?:\.[0-9a-zA-Z-]+)*))?$'
)


if __name__ == '__main__':
    version = dunamai.get_version(
        'gauge',
        third_choice=lambda: dunamai.Version.from_any_vcs(
            pattern=SEMVER_PATTERN
        )
    ).serialize(style=dunamai.Style.SemVer)
    with open('VERSION', 'w') as version_file:
        version_file.write(version)
