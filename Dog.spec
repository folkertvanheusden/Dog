Name:       Dog
Version:    3.51
Release:    0
Summary:    A chess playing program
License:    MIT
Source0:    %{name}-%{version}.tgz
URL:        https://github.com/folkertvanheusden/Dog

%description
A chess playing program. Requires an 'UCI' compatible chess-board interface like scid or arena or xboard with plyglot.

%prep
%setup -q -n %{name}-%{version}

%build
cd app/src/linux-windows/build
%cmake ..
%cmake_build

%install
mkdir -p %{buildroot}/usr/games
cp app/src/linux-windows/build/redhat-linux-build/Dog %{buildroot}/usr/games

%files
/usr/games/Dog

%changelog
* Wed Sep 24 2025 Folkert van Heusden <folkert@vanheusden.com>
-
