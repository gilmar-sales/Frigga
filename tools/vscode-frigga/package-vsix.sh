#!/usr/bin/env bash
# Build a .vsix without @vscode/vsce (avoids its pnpm install preflight).
set -euo pipefail
cd "$(dirname "$0")"

pnpm run compile

VERSION=$(node -p "require('./package.json').version")
NAME=$(node -p "require('./package.json').name")
PUBLISHER=$(node -p "require('./package.json').publisher")
OUT="${PUBLISHER}.${NAME}-${VERSION}.vsix"
STAGE=$(mktemp -d)
trap 'rm -rf "$STAGE"' EXIT

mkdir -p "$STAGE/extension"
cp package.json "$STAGE/extension/"
cp -r out "$STAGE/extension/"

cat >"$STAGE/extension.vsixmanifest" <<EOF
<?xml version="1.0" encoding="utf-8"?>
<PackageManifest Version="2.0.0" xmlns="http://schemas.microsoft.com/developer/vsx-schema/2011" xmlns:d="http://schemas.microsoft.com/developer/vsx-schema-design/2011">
  <Metadata>
    <Identity Language="en-US" Id="${NAME}" Version="${VERSION}" Publisher="${PUBLISHER}" />
    <DisplayName>Frigga</DisplayName>
    <Description xml:space="preserve">Scaffold gameplay components and systems for Frigga projects</Description>
    <Tags></Tags>
    <Categories>Other</Categories>
    <Properties>
      <Property Id="Microsoft.VisualStudio.Code.Engine" Value="^1.85.0" />
      <Property Id="Microsoft.VisualStudio.Code.ExtensionDependencies" Value="" />
      <Property Id="Microsoft.VisualStudio.Code.ExtensionPack" Value="" />
      <Property Id="Microsoft.VisualStudio.Code.LocalizedLanguages" Value="" />
      <Property Id="Microsoft.VisualStudio.Code.EnabledApiProposals" Value="" />
      <Property Id="Microsoft.VisualStudio.Code.ExecutesCode" Value="true" />
    </Properties>
  </Metadata>
  <Installation>
    <InstallationTarget Id="Microsoft.VisualStudio.Code"/>
  </Installation>
  <Dependencies/>
  <Assets>
    <Asset Type="Microsoft.VisualStudio.Code.Manifest" Path="extension/package.json" Addressable="true" />
  </Assets>
</PackageManifest>
EOF

cat >"$STAGE/[Content_Types].xml" <<'EOF'
<?xml version="1.0" encoding="utf-8"?>
<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">
  <Default Extension=".json" ContentType="application/json"/>
  <Default Extension=".vsixmanifest" ContentType="text/xml"/>
  <Default Extension=".js" ContentType="application/javascript"/>
  <Default Extension=".map" ContentType="application/json"/>
</Types>
EOF

rm -f "$OUT"
(
  cd "$STAGE"
  zip -rq "$OLDPWD/$OUT" extension.vsixmanifest '[Content_Types].xml' extension
)

echo "Created $OUT"
