Name:		libavm-emon
Version:    @VERSION@
Release:    1%{?dist}
Summary:    EM-3.1KM (EMon) power-quality meter protocol library for AVM-Energo projects

License:    GPLv3
URL:        https://git.avmenergo.ru/avm-energo/libavm-emon.git

BuildArch:  @ARCH@
Provides:   libavm-emon

%description
EM-3.1KM (EMon) power-quality meter protocol library for AVM-Energo projects

%package devel
Summary:    EM-3.1KM (EMon) power-quality meter protocol library development files for AVM-Energo projects
Requires:   libavm-emon = %{version}-%{release}

%description devel
EM-3.1KM (EMon) power-quality meter protocol library development files for AVM-Energo projects

%prep
# sources pre-built by cmake_build TeamCity step

%build
# build performed by cmake_build TeamCity step

%install
# files pre-installed to BUILDROOT by build_rpm TeamCity step

%files
%defattr(-,root,root)
/usr/lib64/libavm-emon.so*

%files devel
%defattr(-,root,root)
/usr/lib64/libavm-emon.a
/usr/lib64/cmake/libavm-emon/
/usr/include/libavm-emon/

%changelog
* Wed Aug 19 2026 AVM-Energo <support@avmenergo.ru>
- Initial release: GetDeviceInfo, SetLimits, GetAnalogInputs over RS-232 and USB
