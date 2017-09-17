use strict;
use XML::LibXML; use Data::Dumper;
use Scalar::Util qw(looks_like_number);

my %data;

# process simple XML documents (consisting of record elements only)
sub simpleXML($$) {
	my ($fname, $prefix) = @_;
	print "processing $fname\n";
	open SQLOUT, ">$fname.sql";
	print SQLOUT "use ORE\n\n";
	my $xmldata= XML::LibXML->load_xml(location => './'."$fname.xml", no_blanks => 1);
	my @firstlevel=$xmldata->firstChild->childNodes;
	foreach my $record (sort @firstlevel) {
		if (ref($record) eq "XML::LibXML::Element") {
			my $tableName = $record->nodeName;
			printInsert($record, $prefix, $tableName);
		}
	}
	close SQLOUT;
}
simpleXML("conventions","Conventions");

# process the pricingengine data
open SQLOUT, ">pricingengine.sql";
print SQLOUT "use ORE\n\n";
my $xmldata= XML::LibXML->load_xml(location => './pricingengine.xml', no_blanks => 1);
print "processing PricingEngine Data\n";
my @firstlevel = $xmldata->firstChild->childNodes;
foreach my $record (@firstlevel) {
	printInsert($record, "PricingEngine", "Products");
	my $typeAtt = $record->getAttribute("type");
	my @subrecordData = $record->findnodes('EngineParameters/Parameter');
	foreach my $subrecord (@subrecordData) {
		$subrecord->setAttribute("type", $typeAtt);
		printInsert($subrecord, "PricingEngine", "EngineParameters");
	}
	my @subrecordData = $record->findnodes('ModelParameters/Parameter');
	foreach my $subrecord (@subrecordData) {
		$subrecord->setAttribute("type", $typeAtt);
		printInsert($subrecord, "PricingEngine", "ModelParameters");
	}
}
close SQLOUT;


# process the ore_types data
open SQLOUT, ">ore_types.sql";
print SQLOUT "use ORE\n\n";
my $xmldata= XML::LibXML->load_xml(location => './ore_types.xsd', no_blanks => 1);
print "processing oretypes\n";
#<xs:schema attributeFormDefault="unqualified" elementFormDefault="qualified" xmlns:xs="http://www.w3.org/2001/XMLSchema">
#	<xs:simpleType name="bool">
#	...
#	<xs:simpleType name="currencyCode">
#	..
my @firstlevel = $xmldata->firstChild->childNodes;
foreach my $record (@firstlevel) {
	if (ref($record) eq "XML::LibXML::Element") {
	my $tableName = $record->getAttribute("name");
		#<xs:simpleType name="bool">
		#	<xs:restriction base="xs:string">
		#		<xs:enumeration value="Y"/>
		my @subrecordData = $record->firstChild->childNodes;
		foreach my $subrecord (@subrecordData) {
			printInsert($subrecord, "Types", $tableName, 1);
		}
	}
}
close SQLOUT;


# helper functions
# print the insert statement for a XML Record
sub printInsert() {
	my ($record, $prefix, $tableName, $numericAsChar) = @_;
	
	my ($colNames, $colValues);
	#first the element data
	foreach my $tableCol ($record->childNodes) {
		#print $tableCol->nodeName.":".ref($tableCol).",tcolexists:".$tableCol->exists('*').",textContent:".$tableCol->textContent."\n";
		# real nodes/elements (no comments, etc.)    && no subnodes (exists(*)) && no empty nodes
		if (ref($tableCol) eq "XML::LibXML::Element" && !$tableCol->exists('*') && $tableCol->textContent ne '' && $tableCol->textContent ne "") {
			$colNames.=($tableCol->nodeName =~ /^Rule$|^Index$/ ? $tableCol->nodeName."Name" : $tableCol->nodeName).",";
			$colValues.=formatSQL($tableCol->textContent, $numericAsChar).",";
		}
		# plain text nodes
		if (ref($tableCol) eq "XML::LibXML::Text" && $tableCol->textContent ne '' && $tableCol->textContent ne "") {
			$colNames.=($tableCol->parentNode->nodeName =~ /^Rule$|^Index$/ ? $tableCol->parentNode->nodeName."Name" : $tableCol->parentNode->nodeName).",";
			$colValues.=formatSQL($tableCol->textContent, $numericAsChar).",";
		}
	}
	# then the attributes
	if ($record->hasAttributes) {
		foreach my $tableCol ($record->attributes) {
			#print Dumper($tableCol)."\n";
			if ($tableCol->value ne "") {
				$colNames.=($tableCol->nodeName =~ /^Rule$|^Index$/ ? $tableCol->nodeName."Name" : $tableCol->nodeName).",";
				$colValues.=formatSQL($tableCol->value, $numericAsChar).",";
			}
		}
	}
	print SQLOUT "INSERT $prefix$tableName (".substr($colNames,0,-1).") VALUES (".substr($colValues,0,-1).")\n" if $colValues ne "";
}

# format SQL according to type (number vs. string/dates)
sub formatSQL() {
	my ($var, $numericAsChar) = @_;
	return $var if looks_like_number($var) && !$numericAsChar;
	return "'".$var."'";
}