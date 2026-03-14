// Copyright 2020 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "escl_manager.h"

#include <cstdint>
#include <optional>
#include <utility>

#include <base/files/scoped_temp_dir.h>
#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/values.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#include "scan_xml_util.h"
#include "scanner.h"

using base::Value;
using ::testing::ElementsAre;

namespace {

// An eSCL ScanSettings structure serialized to XML. This is used to test our
// XML parsing code, as well as to test handling of requests to the ScanJob
// endpoint that wish to create new scans.
// clang-format off
constexpr char kNewScan[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    "<scan:ScanSettings xmlns:pwg=\"http://www.pwg.org/schemas/2010/12/sm\" "
         "xmlns:scan=\"http://schemas.hp.com/imaging/escl/2011/05/03\">"
       "<pwg:Version>2.0</pwg:Version>"
       "<pwg:ScanRegions>"
           "<pwg:ScanRegion>"
               "<pwg:ContentRegionUnits>"
                   "escl:ThreeHundredthsOfInches"
               "</pwg:ContentRegionUnits>"
               "<pwg:Height>600</pwg:Height>"
               "<pwg:Width>200</pwg:Width>"
               "<pwg:XOffset>0</pwg:XOffset>"
               "<pwg:YOffset>0</pwg:YOffset>"
           "</pwg:ScanRegion>"
       "</pwg:ScanRegions>"
       "<pwg:DocumentFormat>application/pdf</pwg:DocumentFormat>"
       "<scan:ColorMode>RGB24</scan:ColorMode>"
       "<scan:XResolution>300</scan:XResolution>"
       "<scan:YResolution>300</scan:YResolution>"
       "<pwg:InputSource>Platen</pwg:InputSource>"
       "<scan:InputSource>Platen</scan:InputSource>"
   "</scan:ScanSettings>";
constexpr char kScanInvalidResolution[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    "<scan:ScanSettings xmlns:pwg=\"http://www.pwg.org/schemas/2010/12/sm\" "
         "xmlns:scan=\"http://schemas.hp.com/imaging/escl/2011/05/03\">"
       "<pwg:Version>2.0</pwg:Version>"
       "<pwg:ScanRegions>"
           "<pwg:ScanRegion>"
               "<pwg:ContentRegionUnits>"
                   "escl:ThreeHundredthsOfInches"
               "</pwg:ContentRegionUnits>"
               "<pwg:Height>600</pwg:Height>"
               "<pwg:Width>200</pwg:Width>"
               "<pwg:XOffset>0</pwg:XOffset>"
               "<pwg:YOffset>0</pwg:YOffset>"
           "</pwg:ScanRegion>"
       "</pwg:ScanRegions>"
       "<pwg:DocumentFormat>application/pdf</pwg:DocumentFormat>"
       "<scan:ColorMode>RGB24</scan:ColorMode>"
       "<scan:XResolution>100</scan:XResolution>"
       "<scan:YResolution>300</scan:YResolution>"
       "<pwg:InputSource>Platen</pwg:InputSource>"
       "<scan:InputSource>Platen</scan:InputSource>"
   "</scan:ScanSettings>";
constexpr char kScanUnsupportedInvalidResolution[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    "<scan:ScanSettings xmlns:pwg=\"http://www.pwg.org/schemas/2010/12/sm\" "
         "xmlns:scan=\"http://schemas.hp.com/imaging/escl/2011/05/03\">"
       "<pwg:Version>2.0</pwg:Version>"
       "<pwg:ScanRegions>"
           "<pwg:ScanRegion>"
               "<pwg:ContentRegionUnits>"
                   "escl:ThreeHundredthsOfInches"
               "</pwg:ContentRegionUnits>"
               "<pwg:Height>600</pwg:Height>"
               "<pwg:Width>200</pwg:Width>"
               "<pwg:XOffset>0</pwg:XOffset>"
               "<pwg:YOffset>0</pwg:YOffset>"
           "</pwg:ScanRegion>"
       "</pwg:ScanRegions>"
       "<pwg:DocumentFormat>application/pdf</pwg:DocumentFormat>"
       "<scan:ColorMode>RGB24</scan:ColorMode>"
       "<scan:XResolution>400</scan:XResolution>"
       "<scan:YResolution>400</scan:YResolution>"
       "<pwg:InputSource>Platen</pwg:InputSource>"
       "<scan:InputSource>Platen</scan:InputSource>"
   "</scan:ScanSettings>";
constexpr char kScanFeeder[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    "<scan:ScanSettings xmlns:pwg=\"http://www.pwg.org/schemas/2010/12/sm\" "
         "xmlns:scan=\"http://schemas.hp.com/imaging/escl/2011/05/03\">"
       "<pwg:Version>2.0</pwg:Version>"
       "<pwg:ScanRegions>"
           "<pwg:ScanRegion>"
               "<pwg:ContentRegionUnits>"
                   "escl:ThreeHundredthsOfInches"
               "</pwg:ContentRegionUnits>"
               "<pwg:Height>600</pwg:Height>"
               "<pwg:Width>200</pwg:Width>"
               "<pwg:XOffset>0</pwg:XOffset>"
               "<pwg:YOffset>0</pwg:YOffset>"
           "</pwg:ScanRegion>"
       "</pwg:ScanRegions>"
       "<pwg:DocumentFormat>application/pdf</pwg:DocumentFormat>"
       "<scan:ColorMode>RGB24</scan:ColorMode>"
       "<scan:XResolution>300</scan:XResolution>"
       "<scan:YResolution>300</scan:YResolution>"
       "<pwg:InputSource>Feeder</pwg:InputSource>"
       "<scan:InputSource>Feeder</scan:InputSource>"
   "</scan:ScanSettings>";
constexpr char kScanInvalidSource[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    "<scan:ScanSettings xmlns:pwg=\"http://www.pwg.org/schemas/2010/12/sm\" "
         "xmlns:scan=\"http://schemas.hp.com/imaging/escl/2011/05/03\">"
       "<pwg:Version>2.0</pwg:Version>"
       "<pwg:ScanRegions>"
           "<pwg:ScanRegion>"
               "<pwg:ContentRegionUnits>"
                   "escl:ThreeHundredthsOfInches"
               "</pwg:ContentRegionUnits>"
               "<pwg:Height>600</pwg:Height>"
               "<pwg:Width>200</pwg:Width>"
               "<pwg:XOffset>0</pwg:XOffset>"
               "<pwg:YOffset>0</pwg:YOffset>"
           "</pwg:ScanRegion>"
       "</pwg:ScanRegions>"
       "<pwg:DocumentFormat>application/pdf</pwg:DocumentFormat>"
       "<scan:ColorMode>RGB24</scan:ColorMode>"
       "<scan:XResolution>300</scan:XResolution>"
       "<scan:YResolution>300</scan:YResolution>"
       "<pwg:InputSource>Invalid</pwg:InputSource>"
       "<scan:InputSource>Invalid</scan:InputSource>"
   "</scan:ScanSettings>";
// clang-format on

base::DictValue CreateCapabilitiesJson(bool createAdf = true) {
  base::DictValue dict;
  dict.Set("MakeAndModel", "Test Make and Model");
  dict.Set("SerialNumber", "Test Serial");

  base::DictValue platen;
  platen.Set("MaxWidth", 3660);
  platen.Set("MaxHeight", 5160);

  base::ListValue platen_color_modes;
  platen_color_modes.Append("RGB24");
  platen_color_modes.Append("Grayscale8");
  platen.Set("ColorModes", std::move(platen_color_modes));

  base::ListValue platen_document_formats;
  platen_document_formats.Append("application/pdf");
  platen.Set("DocumentFormats", std::move(platen_document_formats));

  base::ListValue platen_resolutions;
  platen_resolutions.Append(100);
  platen_resolutions.Append(200);
  platen_resolutions.Append(300);
  platen.Set("Resolutions", std::move(platen_resolutions));

  dict.Set("Platen", std::move(platen));

  if (createAdf) {
    base::DictValue adf;
    adf.Set("MaxWidth", 3660);
    adf.Set("MaxHeight", 5160);

    base::ListValue adf_color_modes;
    adf_color_modes.Append("BlackAndWhite1");
    adf_color_modes.Append("Grayscale8");
    adf.Set("ColorModes", std::move(adf_color_modes));

    base::ListValue adf_document_formats;
    adf_document_formats.Append("image/jpeg");
    adf.Set("DocumentFormats", std::move(adf_document_formats));

    base::ListValue adf_resolutions;
    adf_resolutions.Append(75);
    adf_resolutions.Append(150);
    adf_resolutions.Append(600);
    adf.Set("Resolutions", std::move(adf_resolutions));

    adf.Set("XJustification", "Right");

    dict.Set("ADF", std::move(adf));
  }

  return dict;
}

inline const xmlChar* XmlCast(const char* p) {
  return reinterpret_cast<const xmlChar*>(p);
}

std::string GetCapLogStr() {
  return "{"
         "\"ColorMode\":2,"
         "\"DocumentFormat\":\"application/pdf\","
         "\"InputSource\":\"Platen\","
         "\"Regions\":[{"
         "\"Height\":600,"
         "\"Units\":\"escl:ThreeHundredthsOfInches\","
         "\"Width\":200,"
         "\"XOffset\":0,"
         "\"YOffset\":0"
         "}],"
         "\"XResolution\":300,"
         "\"YResolution\":300"
         "}";
}

}  // namespace

TEST(ScannerCapabilities, Initialize) {
  std::optional<ScannerCapabilities> opt_caps =
      Scanner::CreateScannerCapabilitiesFromConfig(CreateCapabilitiesJson());
  ASSERT_TRUE(opt_caps);

  ScannerCapabilities caps = opt_caps.value();
  EXPECT_EQ(caps.make_and_model, "Test Make and Model");
  EXPECT_EQ(caps.serial_number, "Test Serial");
  EXPECT_THAT(caps.platen_capabilities.color_modes,
              ElementsAre("RGB24", "Grayscale8"));
  EXPECT_EQ(caps.platen_capabilities.max_width, 3660);
  EXPECT_EQ(caps.platen_capabilities.max_height, 5160);
  EXPECT_THAT(caps.platen_capabilities.formats, ElementsAre("application/pdf"));
  EXPECT_THAT(caps.platen_capabilities.resolutions, ElementsAre(100, 200, 300));
  ASSERT_TRUE(caps.adf_capabilities.has_value());
  EXPECT_EQ(caps.adf_capabilities.value().max_width, 3660);
  EXPECT_EQ(caps.adf_capabilities.value().max_height, 5160);
  EXPECT_THAT(caps.adf_capabilities.value().color_modes,
              ElementsAre("BlackAndWhite1", "Grayscale8"));
  EXPECT_THAT(caps.adf_capabilities.value().formats, ElementsAre("image/jpeg"));
  EXPECT_THAT(caps.adf_capabilities.value().resolutions,
              ElementsAre(75, 150, 600));
  EXPECT_EQ(caps.adf_capabilities.value().x_justification, "Right");
}

TEST(ScannerCapabilities, InitializeFailColorModeHasInteger) {
  base::DictValue json = CreateCapabilitiesJson();
  json.FindListByDottedPath("Platen.ColorModes")->Append(Value(9));
  EXPECT_FALSE(Scanner::CreateScannerCapabilitiesFromConfig(std::move(json)));
}

TEST(ScannerCapabilities, InitializeFailDocumentFormatsHasDouble) {
  base::DictValue json = CreateCapabilitiesJson();
  json.FindListByDottedPath("Platen.DocumentFormats")->Append(Value(2.7));
  EXPECT_FALSE(Scanner::CreateScannerCapabilitiesFromConfig(std::move(json)));
}

TEST(ScannerCapabilities, InitializeFailResolutionsHasString) {
  base::DictValue json = CreateCapabilitiesJson();
  json.FindListByDottedPath("Platen.Resolutions")->Append(Value("600"));
  EXPECT_FALSE(Scanner::CreateScannerCapabilitiesFromConfig(std::move(json)));
}

TEST(ScannerCapabilities, InitializeFailMissingMakeAndModel) {
  base::DictValue json = CreateCapabilitiesJson();
  EXPECT_TRUE(json.Remove("MakeAndModel"));
  EXPECT_FALSE(Scanner::CreateScannerCapabilitiesFromConfig(std::move(json)));
}

TEST(ScannerCapabilities, InitializeFailMissingSerialNumber) {
  base::DictValue json = CreateCapabilitiesJson();
  EXPECT_TRUE(json.Remove("SerialNumber"));
  EXPECT_FALSE(Scanner::CreateScannerCapabilitiesFromConfig(std::move(json)));
}

TEST(ScannerCapabilities, InitializeFailMissingColorModes) {
  base::DictValue json = CreateCapabilitiesJson();
  EXPECT_TRUE(json.RemoveByDottedPath("Platen.ColorModes"));
  EXPECT_FALSE(Scanner::CreateScannerCapabilitiesFromConfig(std::move(json)));
}

TEST(ScannerCapabilities, InitializeFailMissingDocumentFormats) {
  base::DictValue json = CreateCapabilitiesJson();
  EXPECT_TRUE(json.RemoveByDottedPath("Platen.DocumentFormats"));
  EXPECT_FALSE(Scanner::CreateScannerCapabilitiesFromConfig(std::move(json)));
}

TEST(ScannerCapabilities, InitializeFailMissingResolutions) {
  base::DictValue json = CreateCapabilitiesJson();
  EXPECT_TRUE(json.RemoveByDottedPath("Platen.Resolutions"));
  EXPECT_FALSE(Scanner::CreateScannerCapabilitiesFromConfig(std::move(json)));
}

TEST(ScannerCapabilities, InitializeFailMissingPlatenSection) {
  base::DictValue json = CreateCapabilitiesJson();
  EXPECT_TRUE(json.Remove("Platen"));
  EXPECT_FALSE(Scanner::CreateScannerCapabilitiesFromConfig(std::move(json)));
}

TEST(ScannerCapabilities, InitializePassMissingMaxWidth) {
  base::DictValue json = CreateCapabilitiesJson();
  EXPECT_TRUE(json.RemoveByDottedPath("Platen.MaxWidth"));
  EXPECT_TRUE(json.RemoveByDottedPath("ADF.MaxWidth"));
  EXPECT_TRUE(Scanner::CreateScannerCapabilitiesFromConfig(std::move(json)));
}

TEST(ScannerCapabilities, InitializePassMissingMaxHeight) {
  base::DictValue json = CreateCapabilitiesJson();
  EXPECT_TRUE(json.RemoveByDottedPath("Platen.MaxHeight"));
  EXPECT_TRUE(json.RemoveByDottedPath("ADF.MaxHeight"));
  EXPECT_TRUE(Scanner::CreateScannerCapabilitiesFromConfig(std::move(json)));
}

TEST(ScannerCapabilities, InitializePassMissingJustification) {
  base::DictValue json = CreateCapabilitiesJson();
  EXPECT_TRUE(json.RemoveByDottedPath("ADF.XJustification"));
  EXPECT_TRUE(Scanner::CreateScannerCapabilitiesFromConfig(std::move(json)));
}

// Despite taking a mutable pointer to |doc|, this does not modify |doc|.
void HasPathWithContents(xmlDoc* doc,
                         const std::string& path,
                         std::vector<std::string> expected_contents) {
  xmlXPathContext* context = xmlXPathNewContext(doc);
  xmlXPathRegisterNs(context, XmlCast("scan"),
                     XmlCast("http://schemas.hp.com/imaging/escl/2011/05/03"));
  xmlXPathRegisterNs(context, XmlCast("pwg"),
                     XmlCast("http://www.pwg.org/schemas/2010/12/sm"));
  xmlXPathObject* result =
      xmlXPathEvalExpression(XmlCast(path.c_str()), context);
  ASSERT_FALSE(xmlXPathNodeSetIsEmpty(result->nodesetval))
      << "No nodes found with path " << path;
  int node_count = result->nodesetval->nodeNr;
  ASSERT_EQ(node_count, expected_contents.size())
      << "Found " << node_count << " nodes, but " << expected_contents.size()
      << " were expected.";
  for (int i = 0; i < node_count; i++) {
    const xmlNode* node = result->nodesetval->nodeTab[i];
    xmlChar* node_text = xmlNodeGetContent(node);
    EXPECT_EQ(reinterpret_cast<char*>(node_text), expected_contents[i]);
    xmlFree(node_text);
  }
  xmlXPathFreeContext(context);
  xmlXPathFreeObject(result);
}

TEST(ScannerCapabilities, AsXml) {
  std::optional<ScannerCapabilities> caps =
      Scanner::CreateScannerCapabilitiesFromConfig(CreateCapabilitiesJson());
  ASSERT_TRUE(caps);
  std::vector<uint8_t> xml = ScannerCapabilitiesAsXml(caps.value());
  xmlDoc* doc = xmlReadMemory(reinterpret_cast<const char*>(xml.data()),
                              xml.size(), "noname.xml", NULL, 0);
  EXPECT_NE(doc, nullptr);

  HasPathWithContents(doc, "/scan:ScannerCapabilities/pwg:Version", {"2.63"});
  HasPathWithContents(doc, "/scan:ScannerCapabilities/pwg:MakeAndModel",
                      {"Test Make and Model"});
  HasPathWithContents(doc, "/scan:ScannerCapabilities/pwg:SerialNumber",
                      {"Test Serial"});

  // Ensure that the reported Platen capabilities are:
  // MaxWidth: 3660
  // MaxHeight: 5160
  // Color modes: Grayscale8, RGB24
  // Document formats: application/pdf
  // Discrete Resolutions: 100, 200, 300
  const std::string platen_caps =
      "/scan:ScannerCapabilities/scan:Platen/scan:PlatenInputCaps";
  HasPathWithContents(doc, platen_caps + "/scan:MaxWidth", {"3660"});
  HasPathWithContents(doc, platen_caps + "/scan:MaxHeight", {"5160"});

  const std::string profiles = "/scan:SettingProfiles/scan:SettingProfile";
  HasPathWithContents(
      doc, platen_caps + profiles + "/scan:ColorModes/scan:ColorMode",
      {"RGB24", "Grayscale8"});
  HasPathWithContents(
      doc, platen_caps + profiles + "/scan:DocumentFormats/pwg:DocumentFormat",
      {"application/pdf"});
  HasPathWithContents(doc,
                      platen_caps + profiles +
                          "/scan:SupportedResolutions/scan:DiscreteResolutions/"
                          "scan:DiscreteResolution/scan:XResolution",
                      {"100", "200", "300"});

  // Ensure that the reported ADF simplex capabilities are:
  // MaxWidth: 3660
  // MaxHeight: 5160
  // Color modes: BlackAndWhite1, Grayscale8
  // Document formats: image/jpeg
  // Discrete Resolutions: 75, 150, 600
  // X Justification: Right
  const std::string adf_simplex_caps =
      "/scan:ScannerCapabilities/scan:Adf/scan:AdfSimplexInputCaps";
  HasPathWithContents(doc, adf_simplex_caps + "/scan:MaxWidth", {"3660"});
  HasPathWithContents(doc, adf_simplex_caps + "/scan:MaxHeight", {"5160"});

  HasPathWithContents(
      doc, adf_simplex_caps + profiles + "/scan:ColorModes/scan:ColorMode",
      {"BlackAndWhite1", "Grayscale8"});
  HasPathWithContents(
      doc,
      adf_simplex_caps + profiles + "/scan:DocumentFormats/pwg:DocumentFormat",
      {"image/jpeg"});
  HasPathWithContents(doc,
                      adf_simplex_caps + profiles +
                          "/scan:SupportedResolutions/scan:DiscreteResolutions/"
                          "scan:DiscreteResolution/scan:XResolution",
                      {"75", "150", "600"});
  HasPathWithContents(doc,
                      adf_simplex_caps + profiles +
                          "/scan:SupportedResolutions/scan:DiscreteResolutions/"
                          "scan:DiscreteResolution/scan:YResolution",
                      {"75", "150", "600"});

  // Ensure that the reported ADF duplex capabilities are:
  // MaxWidth: 3660
  // MaxHeight: 5160
  // Color modes: BlackAndWhite1, Grayscale8
  // Document formats: image/jpeg
  // Discrete Resolutions: 75, 150, 600
  const std::string adf_duplex_caps =
      "/scan:ScannerCapabilities/scan:Adf/scan:AdfDuplexInputCaps";
  HasPathWithContents(doc, adf_duplex_caps + "/scan:MaxWidth", {"3660"});
  HasPathWithContents(doc, adf_duplex_caps + "/scan:MaxHeight", {"5160"});

  HasPathWithContents(
      doc, adf_duplex_caps + profiles + "/scan:ColorModes/scan:ColorMode",
      {"BlackAndWhite1", "Grayscale8"});
  HasPathWithContents(
      doc,
      adf_duplex_caps + profiles + "/scan:DocumentFormats/pwg:DocumentFormat",
      {"image/jpeg"});
  HasPathWithContents(doc,
                      adf_duplex_caps + profiles +
                          "/scan:SupportedResolutions/scan:DiscreteResolutions/"
                          "scan:DiscreteResolution/scan:XResolution",
                      {"75", "150", "600"});
  HasPathWithContents(doc,
                      adf_duplex_caps + profiles +
                          "/scan:SupportedResolutions/scan:DiscreteResolutions/"
                          "scan:DiscreteResolution/scan:YResolution",
                      {"75", "150", "600"});

  HasPathWithContents(doc,
                      "/scan:ScannerCapabilities/scan:Adf/scan:Justification/"
                      "pwg:XImagePosition",
                      {"Right"});

  xmlFreeDoc(doc);
}

// Test that we can properly parse a ScanSettings document.
TEST(ScanSettings, Parse) {
  std::vector<uint8_t> xml(kNewScan, kNewScan + strlen(kNewScan));
  std::optional<ScanSettings> opt_settings = ScanSettingsFromXml(xml);
  EXPECT_TRUE(opt_settings);
  ScanSettings settings = opt_settings.value();
  EXPECT_EQ(settings.document_format, "application/pdf");
  EXPECT_EQ(settings.color_mode, kRGB);
  EXPECT_EQ(settings.input_source, "Platen");
  EXPECT_EQ(settings.x_resolution, 300);
  EXPECT_EQ(settings.y_resolution, 300);

  EXPECT_EQ(settings.regions.size(), 1);
  ScanRegion region = settings.regions.at(0);
  EXPECT_EQ(region.units, "escl:ThreeHundredthsOfInches");
  EXPECT_EQ(region.height, 600);
  EXPECT_EQ(region.width, 200);
  EXPECT_EQ(region.x_offset, 0);
  EXPECT_EQ(region.y_offset, 0);
}

TEST(HandleEsclRequest, InvalidEndpoint) {
  HttpRequest request;
  request.method = "GET";
  request.uri = "/eSCL/InvalidEndpoint";

  ScannerCapabilities caps;
  EsclManager manager(caps, base::FilePath());
  HttpResponse response = manager.HandleEsclRequest(request, SmartBuffer());
  EXPECT_EQ(response.status, "404 Not Found");
  EXPECT_EQ(response.body.size(), 0);
}

// Tests that a GET request to /eSCL/ScannerCapabilities returns a valid XML
// response.
// TODO(b/157735732): add validation logic once we can.
TEST(HandleEsclRequest, ScannerCapabilities) {
  HttpRequest request;
  request.method = "GET";
  request.uri = "/eSCL/ScannerCapabilities";

  std::optional<ScannerCapabilities> caps =
      Scanner::CreateScannerCapabilitiesFromConfig(CreateCapabilitiesJson());
  ASSERT_TRUE(caps);
  EsclManager manager(caps.value(), base::FilePath());
  HttpResponse response = manager.HandleEsclRequest(request, SmartBuffer());
  EXPECT_EQ(response.status, "200 OK");
  EXPECT_GT(response.body.size(), 0);

  xmlDoc* doc =
      xmlReadMemory(reinterpret_cast<const char*>(response.body.data()),
                    response.body.size(), "noname.xml", NULL, 0);
  EXPECT_NE(doc, nullptr);
  xmlFreeDoc(doc);
}

// Tests that a GET request to /eSCL/ScannerStatus returns a valid XML
// response.
// TODO(b/157735732): add validation logic once we can.
TEST(HandleEsclRequest, ScannerStatus) {
  HttpRequest request;
  request.method = "GET";
  request.uri = "/eSCL/ScannerStatus";

  ScannerCapabilities caps;
  EsclManager manager(caps, base::FilePath());
  HttpResponse response = manager.HandleEsclRequest(request, SmartBuffer());
  EXPECT_EQ(response.status, "200 OK");
  EXPECT_GT(response.body.size(), 0);

  xmlDoc* doc =
      xmlReadMemory(reinterpret_cast<const char*>(response.body.data()),
                    response.body.size(), "noname.xml", NULL, 0);
  EXPECT_NE(doc, nullptr);
  xmlFreeDoc(doc);
}

TEST(HandleEsclRequest, GetJobsInvalidRequest) {
  HttpRequest request;
  request.method = "GET";
  // This uri is invalid when trying to get the next job.
  request.uri = "/eSCL/ScanJobs/";

  ScannerCapabilities caps;
  EsclManager manager(caps, base::FilePath());
  HttpResponse response = manager.HandleEsclRequest(request, SmartBuffer());
  EXPECT_EQ(response.status, "405 Method Not Allowed");
  EXPECT_EQ(response.body.size(), 0);
}

TEST(HandleEsclRequest, GetJobsInvalidJobId) {
  HttpRequest request;
  request.method = "GET";
  // An invalid id
  request.uri = "/eSCL/ScanJobs/12345/NextDocument";

  ScannerCapabilities caps;
  EsclManager manager(caps, base::FilePath());
  HttpResponse response = manager.HandleEsclRequest(request, SmartBuffer());
  EXPECT_EQ(response.status, "404 Not Found");
  EXPECT_EQ(response.body.size(), 0);
}

TEST(HandleEsclRequest, ScanJobs) {
  HttpRequest request;
  request.method = "POST";
  request.uri = "/eSCL/ScanJobs";

  std::optional<ScannerCapabilities> caps =
      Scanner::CreateScannerCapabilitiesFromConfig(CreateCapabilitiesJson());
  ASSERT_TRUE(caps);
  EsclManager manager(caps.value(), base::FilePath());
  std::vector<uint8_t> xml(kNewScan, kNewScan + strlen(kNewScan));
  SmartBuffer body(xml);

  HttpResponse response = manager.HandleEsclRequest(request, body);
  EXPECT_EQ(response.status, "201 Created");
}

TEST(HandleEsclRequest, ScanJobsInvalidXmlRequest) {
  HttpRequest request;
  request.method = "POST";
  request.uri = "/eSCL/ScanJobs";

  std::optional<ScannerCapabilities> caps =
      Scanner::CreateScannerCapabilitiesFromConfig(CreateCapabilitiesJson());
  ASSERT_TRUE(caps);
  EsclManager manager(caps.value(), base::FilePath());
  char invalidRequestXml[] = "Invalid XML";
  std::vector<uint8_t> xml(invalidRequestXml,
                           invalidRequestXml + strlen(invalidRequestXml));
  SmartBuffer body(xml);

  HttpResponse response = manager.HandleEsclRequest(request, body);
  EXPECT_EQ(response.status, "415 Unsupported Media Type");
}

TEST(HandleEsclRequest, ScanInvalidResolution) {
  HttpRequest request;
  request.method = "POST";
  request.uri = "/eSCL/ScanJobs";

  std::optional<ScannerCapabilities> caps =
      Scanner::CreateScannerCapabilitiesFromConfig(CreateCapabilitiesJson());
  ASSERT_TRUE(caps);
  EsclManager manager(caps.value(), base::FilePath());
  std::vector<uint8_t> xml(
      kScanInvalidResolution,
      kScanInvalidResolution + strlen(kScanInvalidResolution));
  SmartBuffer body(xml);

  HttpResponse response = manager.HandleEsclRequest(request, body);
  EXPECT_EQ(response.status, "409 Conflict");
}

TEST(HandleEsclRequest, ScanUnsupportedResolution) {
  HttpRequest request;
  request.method = "POST";
  request.uri = "/eSCL/ScanJobs";

  std::optional<ScannerCapabilities> caps =
      Scanner::CreateScannerCapabilitiesFromConfig(CreateCapabilitiesJson());
  ASSERT_TRUE(caps);
  EsclManager manager(caps.value(), base::FilePath());
  std::vector<uint8_t> xml(kScanUnsupportedInvalidResolution,
                           kScanUnsupportedInvalidResolution +
                               strlen(kScanUnsupportedInvalidResolution));
  SmartBuffer body(xml);

  HttpResponse response = manager.HandleEsclRequest(request, body);
  EXPECT_EQ(response.status, "409 Conflict");
}

TEST(HandleEsclRequest, ScanFeeder) {
  HttpRequest request;
  request.method = "POST";
  request.uri = "/eSCL/ScanJobs";

  std::optional<ScannerCapabilities> caps =
      Scanner::CreateScannerCapabilitiesFromConfig(CreateCapabilitiesJson());
  ASSERT_TRUE(caps);
  EsclManager manager(caps.value(), base::FilePath());
  std::vector<uint8_t> xml(kScanFeeder, kScanFeeder + strlen(kScanFeeder));
  SmartBuffer body(xml);

  HttpResponse response = manager.HandleEsclRequest(request, body);
  EXPECT_EQ(response.status, "409 Conflict");
}

TEST(HandleEsclRequest, ScanNoFeederCapabilities) {
  HttpRequest request;
  request.method = "POST";
  request.uri = "/eSCL/ScanJobs";

  std::optional<ScannerCapabilities> caps =
      Scanner::CreateScannerCapabilitiesFromConfig(
          CreateCapabilitiesJson(false));
  ASSERT_TRUE(caps);
  EsclManager manager(caps.value(), base::FilePath());
  std::vector<uint8_t> xml(kScanFeeder, kScanFeeder + strlen(kScanFeeder));
  SmartBuffer body(xml);

  HttpResponse response = manager.HandleEsclRequest(request, body);
  EXPECT_EQ(response.status, "409 Conflict");
}

TEST(HandleEsclRequest, ScanUnknownSource) {
  HttpRequest request;
  request.method = "POST";
  request.uri = "/eSCL/ScanJobs";

  std::optional<ScannerCapabilities> caps =
      Scanner::CreateScannerCapabilitiesFromConfig(CreateCapabilitiesJson());
  ASSERT_TRUE(caps);
  EsclManager manager(caps.value(), base::FilePath());
  std::vector<uint8_t> xml(kScanInvalidSource,
                           kScanInvalidSource + strlen(kScanInvalidSource));
  SmartBuffer body(xml);

  HttpResponse response = manager.HandleEsclRequest(request, body);
  EXPECT_EQ(response.status, "409 Conflict");
}

TEST(HandleEsclRequest, InvalidScanJobs) {
  HttpRequest request;
  request.method = "POST";
  request.uri = "/eSCL/ScanJobs";

  base::DictValue json = CreateCapabilitiesJson();
  {
    auto* value = json.FindListByDottedPath("Platen.DocumentFormats");
    ASSERT_TRUE(value);
    value->erase(value->end() - 1);
  }
  std::optional<ScannerCapabilities> caps =
      Scanner::CreateScannerCapabilitiesFromConfig(std::move(json));
  ASSERT_TRUE(caps);
  EsclManager manager(caps.value(), base::FilePath());
  std::vector<uint8_t> xml(kNewScan, kNewScan + strlen(kNewScan));
  SmartBuffer body(xml);

  HttpResponse response = manager.HandleEsclRequest(request, body);
  EXPECT_EQ(response.status, "409 Conflict");
}

TEST(HandleEsclRequest, DeleteExistingScanJob) {
  HttpRequest request;
  request.method = "POST";
  request.uri = "/eSCL/ScanJobs";

  std::optional<ScannerCapabilities> caps =
      Scanner::CreateScannerCapabilitiesFromConfig(CreateCapabilitiesJson());
  ASSERT_TRUE(caps);
  EsclManager manager(caps.value(), base::FilePath());
  std::vector<uint8_t> xml(kNewScan, kNewScan + strlen(kNewScan));
  SmartBuffer body(xml);

  HttpResponse response = manager.HandleEsclRequest(request, body);
  EXPECT_EQ(response.status, "201 Created");
  ASSERT_EQ(response.headers.count("Location"), 1);
  request.uri = response.headers["Location"];
  request.method = "DELETE";

  // Deleting the just created resource should succeed.
  response = manager.HandleEsclRequest(request, SmartBuffer());
  EXPECT_EQ(response.status, "200 OK");

  // Deleting the same resource a second time should return Not Found.
  response = manager.HandleEsclRequest(request, SmartBuffer());
  EXPECT_EQ(response.status, "404 Not Found");
}

TEST(HandleEsclRequest, DeleteNonexistentScanJob) {
  HttpRequest request;
  request.method = "DELETE";
  request.uri = "/eSCL/ScanJobs/0b2cdf31-edee-4246-a1ad-07bbe754856b";

  ScannerCapabilities caps;
  EsclManager manager(caps, base::FilePath());
  HttpResponse response = manager.HandleEsclRequest(request, SmartBuffer());
  EXPECT_EQ(response.status, "404 Not Found");
}

TEST(HandleEsclRequest, ScanJobsLogFileProvided) {
  HttpRequest request;
  request.method = "POST";
  request.uri = "/eSCL/ScanJobs";

  std::optional<ScannerCapabilities> caps =
      Scanner::CreateScannerCapabilitiesFromConfig(CreateCapabilitiesJson());
  ASSERT_TRUE(caps);
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  EsclManager manager(caps.value(), temp_dir.GetPath());
  std::vector<uint8_t> xml(kNewScan, kNewScan + strlen(kNewScan));
  SmartBuffer body(xml);

  HttpResponse response = manager.HandleEsclRequest(request, body);
  EXPECT_EQ(response.status, "201 Created");
  base::FilePath log_file(temp_dir.GetPath().value() +
                          "/01_createscanjob.json");
  EXPECT_TRUE(base::PathExists(log_file));

  std::string file_contents;
  EXPECT_TRUE(base::ReadFileToString(log_file, &file_contents));
  EXPECT_EQ(GetCapLogStr(), file_contents);
}
