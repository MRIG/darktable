/*
    This file is part of darktable,
    copyright (c) 2009--2010 johannes hanika.

    darktable is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    darktable is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with darktable.  If not, see <http://www.gnu.org/licenses/>.
*/

/**
 * this is a collection of custom measured color matrices, profiled
 * for darktable (darktable.sf.net), so far all calculated by Pascal de Bruijn.
 */
typedef struct dt_profiled_colormatrix_t
{
  const char *makermodel;
  int rXYZ[3], gXYZ[3], bXYZ[3], white[3];
}
dt_profiled_colormatrix_t;

  // image submitter, chart type, illuminant, comments
static dt_profiled_colormatrix_t dt_profiled_colormatrices[] = {

  // Robert Park, ColorChecker Passport, strobe, well lit
  { "PENTAX K-x",                   { 821548, 337357,  42923}, { 247818, 1042969, -218735}, { -4105, -293045, 1085129}, {792206, 821823, 668640}},

  // Robert Park, ColorChecker Passport, strobe, well lit
  { "PENTAX K-7",                   { 738541, 294037,  28061}, { 316025,  984482, -189682}, { 12543, -185852, 1075027}, {812683, 843994, 682587}},

  // Sven Lindahl, Wolf Faust IT8, direct sunlight, well lit
  { "Canon EOS-1Ds Mark II",        {1078033, 378601, -31113}, { -15396, 1112045, -245743}, {166794, -252411, 1284531}, {681213, 705048, 590790}},

  // Xavier Besse, CMP Digital Target 3, direct sunlight, well lit
  { "Canon EOS 5D Mark II",         { 967590, 399139,  36026}, { -52094,  819046, -232071}, {144455, -143158, 1069305}, {864227, 899139, 741547}},

  // Alberto Ferrante, Wolf Faust IT8, direct sunlight, well lit
  { "Canon EOS 7D",                 { 977829, 294815, -44205}, { 154175, 1238007, -325684}, {103363, -297791, 1397461}, {707291, 741760, 626251}},

  // Roy Niswanger, ColorChecker DC, direct sunlight, experimental
  { "Canon EOS 30D",                { 840195, 148773, -67017}, { 112915, 1104553, -369720}, {240005,  -19562, 1468338}, {827255, 873337, 715317}},

  // Pascal de Bruijn, CMP Digital Target 3, strobe, well lit
  { "Canon EOS 40D",                { 845901, 325760, -13077}, { 110809,  960724, -213577}, { 82230, -218063, 1110229}, {837906, 868393, 705704}},

  // Pascal de Bruijn, CMP Digital Target 3, strobe, well lit
  { "Canon EOS 50D",                {1035110, 365005,  -8057}, {-192184,  930511, -477417}, {189545, -233353, 1360870}, {863983, 888763, 730026}},

  // Pascal de Bruijn, CMP Digital Target 3, strobe, well lit
  { "Canon EOS 350D DIGITAL",       { 784348, 329681, -18875}, { 227249, 1001602, -115692}, { 23834, -270844, 1011185}, {861252, 886368, 721420}},
  { "Canon EOS DIGITAL REBEL XT",   { 784348, 329681, -18875}, { 227249, 1001602, -115692}, { 23834, -270844, 1011185}, {861252, 886368, 721420}},
  { "Canon EOS Kiss Digital N",     { 784348, 329681, -18875}, { 227249, 1001602, -115692}, { 23834, -270844, 1011185}, {861252, 886368, 721420}},

  // Pascal de Bruijn, CMP Digital Target 3, strobe, well lit
  { "Canon EOS 400D DIGITAL",       { 743546, 283783, -16647}, { 256531, 1035355, -117432}, { 36560, -256836, 1013535}, {855698, 880066, 726181}},
  { "Canon EOS DIGITAL REBEL XTi",  { 743546, 283783, -16647}, { 256531, 1035355, -117432}, { 36560, -256836, 1013535}, {855698, 880066, 726181}},
  { "Canon EOS Kiss Digital X",     { 743546, 283783, -16647}, { 256531, 1035355, -117432}, { 36560, -256836, 1013535}, {855698, 880066, 726181}},

  // Pascal de Bruijn, CMP Digital Target 3, strobe, well lit
  { "Canon EOS 450D",               { 960098, 404968,  22842}, { -85114,  855072, -310928}, {159851, -194611, 1164276}, {851379, 871506, 711823}},
  { "Canon EOS DIGITAL REBEL XSi",  { 960098, 404968,  22842}, { -85114,  855072, -310928}, {159851, -194611, 1164276}, {851379, 871506, 711823}},
  { "Canon EOS Kiss Digital X2",    { 960098, 404968,  22842}, { -85114,  855072, -310928}, {159851, -194611, 1164276}, {851379, 871506, 711823}},

  // Robert Park, ColorChecker Passport, strobe, well lit
  { "Canon EOS REBEL T1i",          { 956711, 314590,   1236}, {  27405, 1158569, -346283}, { 95444, -376572, 1260895}, {870087, 898087, 734146}},
  { "Canon EOS 500D",               { 956711, 314590,   1236}, {  27405, 1158569, -346283}, { 95444, -376572, 1260895}, {870087, 898087, 734146}},
  { "Canon EOS Kiss Digital X3",    { 956711, 314590,   1236}, {  27405, 1158569, -346283}, { 95444, -376572, 1260895}, {870087, 898087, 734146}},

  // Robert Park, ColorChecker Passport, strobe, well lit
  { "Canon EOS REBEL T2i",          { 864960, 319305,  36880}, { 160904, 1113586, -251587}, { 68832, -334290, 1143463}, {848404, 883118, 718628}},
  { "Canon EOS 550D",               { 864960, 319305,  36880}, { 160904, 1113586, -251587}, { 68832, -334290, 1143463}, {848404, 883118, 718628}},
  { "Canon EOS Kiss Digital X4",    { 864960, 319305,  36880}, { 160904, 1113586, -251587}, { 68832, -334290, 1143463}, {848404, 883118, 718628}},

  // Artis Rozentals, Wolf Faust IT8, direct sunlight, well lit
  { "Canon PowerShot S60",          { 879990, 321808,  23041}, { 272324, 1104752, -410950}, { 75500, -184097, 1373230}, {702026, 740524, 622131}},

  // Pascal de Bruijn, CMP Digital Target 3, camera strobe, well lit
  { "Canon PowerShot S90",          { 866531, 231995,  55756}, {  76965, 1067474, -461502}, {106369, -243286, 1314529}, {807449, 855270, 690750}},

  // Henrik Andersson, Homebrew ColorChecker, strobe, well lit
  { "NIKON D60",                    { 746475, 318924,   9277}, { 254776,  946991, -130447}, { 63171, -166458, 1029190}, {753220, 787949, 652695}},

  // Henrik Andersson, Homebrew ColorChecker, strobe, well lit
  { "NIKON D90",                    { 855072, 361176,  22751}, { 177414,  963577, -241501}, { 28931, -229019, 1123062}, {751816, 781677, 650024}},

  // Rolf Steinort, Wolf Faust IT8, direct sunlight, well lit
  { "NIKON D200",                   { 878922, 352966,   2914}, { 273575, 1048141, -116302}, { 61661, -171021, 1126297}, {691483, 727142, 615204}},

  // Alexander Rabtchevich, Wolf Faust IT8, direct sunlight, well lit
  { "SONY DSLR-A200",               { 846786, 366302, -22858}, { 311584, 1046249, -107056}, { 54596, -192993, 1191406}, {708405, 744507, 596771}},

  // Stephane Chauveau, Wolf Faust IT8, direct sunlight, well lit
  { "SONY DSLR-A550",               {1031235, 405899,   1572}, { 185623, 1122162, -272659}, {-25528, -329514, 1249969}, {729797, 753586, 633530}},

  // Mark Haun, Wolf Faust IT8, direct sunlight, well lit
  { "OLYMPUS E-PL1",                { 824387, 288086,  -7355}, { 299500, 1148865, -308929}, { 91858, -198425, 1346603}, {720139, 750717, 619751}},

  // Henrik Andersson, Homebrew ColorChecker, camera strobe, well lit
  { "OLYMPUS SP570UZ",              { 780991, 262283,  27969}, { 147522, 1135239, -422974}, {142731, -293610, 1316803}, {769669, 804474, 676895}},

  // Pieter de Boer, CMP Digital Target 3, camera strobe, well lit
  { "KODAK EASYSHARE Z1015 IS",     { 716446, 157928, -39536}, { 288498, 1234573, -412460}, { 43045, -337677, 1385773}, {774048, 823563, 644012}}
};

static const int dt_profiled_colormatrix_cnt = sizeof(dt_profiled_colormatrices)/sizeof(dt_profiled_colormatrix_t);

