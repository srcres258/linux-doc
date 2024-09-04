#!/usr/bin/env python3
# ex: set filetype=python:

"""Generate code to handle XDR typedefs"""

from jinja2 import Environment

from generators import SourceGenerator
from generators import create_jinja2_environment, get_jinja2_template

from xdr_ast import _XdrBasic, _XdrVariableLengthString
from xdr_ast import _XdrFixedLengthOpaque, _XdrVariableLengthOpaque
from xdr_ast import _XdrFixedLengthArray, _XdrVariableLengthArray
from xdr_ast import _XdrOptionalData, _XdrVoid, _XdrBuiltInType
from xdr_ast import _XdrDeclaration, _XdrTypedef, public_apis


def get_kernel_c_type(type_spec: str) -> str:
    """Return C type to be used for an XDR built-in type"""
    xdr_type_to_c_type = {
        "unsigned_hyper": "u64",
        "hyper": "s64",
        "unsigned_long": "u32",
        "long": "s32",
        "unsigned_int": "u32",
        "int": "s32",
        "bool": "bool",
    }
    if isinstance(type_spec, _XdrBuiltInType):
        return xdr_type_to_c_type[type_spec.type_name]
    return type_spec.type_name


def emit_typedef_declaration(environment: Environment, node: _XdrDeclaration) -> None:
    """Emit a declaration pair for one XDR typedef"""
    if node.name not in public_apis:
        return
    if isinstance(node, _XdrBasic):
        template = get_jinja2_template(environment, "declaration", node.template)
        print(
            template.render(
                name=node.name,
                type=get_kernel_c_type(node.spec),
                classifier=node.spec.c_classifier,
            )
        )
    elif isinstance(node, _XdrVariableLengthString):
        template = get_jinja2_template(environment, "declaration", node.template)
        print(template.render(name=node.name))
    elif isinstance(node, _XdrFixedLengthOpaque):
        template = get_jinja2_template(environment, "declaration", node.template)
        print(template.render(name=node.name, size=node.size))
    elif isinstance(node, _XdrVariableLengthOpaque):
        template = get_jinja2_template(environment, "declaration", node.template)
        print(template.render(name=node.name))
    elif isinstance(node, _XdrFixedLengthArray):
        template = get_jinja2_template(environment, "declaration", node.template)
        print(
            template.render(
                name=node.name,
                type=node.spec.type_name,
                size=node.size,
            )
        )
    elif isinstance(node, _XdrVariableLengthArray):
        template = get_jinja2_template(environment, "declaration", node.template)
        print(
            template.render(
                name=node.name,
                type=node.spec.type_name,
                classifier=node.spec.c_classifier,
            )
        )
    elif isinstance(node, _XdrOptionalData):
        raise NotImplementedError("<optional_data> typedef not yet implemented")
    elif isinstance(node, _XdrVoid):
        raise NotImplementedError("<void> typedef not yet implemented")
    else:
        raise NotImplementedError("typedef: type not recognized")


def emit_type_definition(environment: Environment, node: _XdrDeclaration) -> None:
    """Emit a definition for one XDR typedef"""
    if isinstance(node, _XdrBasic):
        template = get_jinja2_template(environment, "definition", node.template)
        print(
            template.render(
                name=node.name,
                type=get_kernel_c_type(node.spec),
                classifier=node.spec.c_classifier,
            )
        )
    elif isinstance(node, _XdrVariableLengthString):
        template = get_jinja2_template(environment, "definition", node.template)
        print(template.render(name=node.name))
    elif isinstance(node, _XdrFixedLengthOpaque):
        template = get_jinja2_template(environment, "definition", node.template)
        print(template.render(name=node.name, size=node.size))
    elif isinstance(node, _XdrVariableLengthOpaque):
        template = get_jinja2_template(environment, "definition", node.template)
        print(template.render(name=node.name))
    elif isinstance(node, _XdrFixedLengthArray):
        template = get_jinja2_template(environment, "definition", node.template)
        print(
            template.render(
                name=node.name,
                type=node.spec.type_name,
                size=node.size,
            )
        )
    elif isinstance(node, _XdrVariableLengthArray):
        template = get_jinja2_template(environment, "definition", node.template)
        print(
            template.render(
                name=node.name,
                type=node.spec.type_name,
                classifier=node.spec.c_classifier,
            )
        )
    elif isinstance(node, _XdrOptionalData):
        raise NotImplementedError("<optional_data> typedef not yet implemented")
    elif isinstance(node, _XdrVoid):
        raise NotImplementedError("<void> typedef not yet implemented")
    else:
        raise NotImplementedError("typedef: type not recognized")


def emit_typedef_decoder(environment: Environment, node: _XdrDeclaration) -> None:
    """Emit a decoder function for one typedef"""
    if isinstance(node, _XdrBasic):
        template = get_jinja2_template(environment, "decoder", node.template)
        print(
            template.render(
                name=node.name,
                type=node.spec.type_name,
            )
        )
    elif isinstance(node, _XdrVariableLengthString):
        template = get_jinja2_template(environment, "decoder", node.template)
        print(
            template.render(
                name=node.name,
                maxsize=node.maxsize,
            )
        )
    elif isinstance(node, _XdrFixedLengthOpaque):
        template = get_jinja2_template(environment, "decoder", node.template)
        print(
            template.render(
                name=node.name,
                size=node.size,
            )
        )
    elif isinstance(node, _XdrVariableLengthOpaque):
        template = get_jinja2_template(environment, "decoder", node.template)
        print(
            template.render(
                name=node.name,
                maxsize=node.maxsize,
            )
        )
    elif isinstance(node, _XdrFixedLengthArray):
        template = get_jinja2_template(environment, "decoder", node.template)
        print(
            template.render(
                name=node.name,
                type=node.spec.type_name,
                size=node.size,
                classifier=node.spec.c_classifier,
            )
        )
    elif isinstance(node, _XdrVariableLengthArray):
        template = get_jinja2_template(environment, "decoder", node.template)
        print(
            template.render(
                name=node.name,
                type=node.spec.type_name,
                maxsize=node.maxsize,
            )
        )
    elif isinstance(node, _XdrOptionalData):
        raise NotImplementedError("<optional_data> typedef not yet implemented")
    elif isinstance(node, _XdrVoid):
        raise NotImplementedError("<void> typedef not yet implemented")
    else:
        raise NotImplementedError("typedef: type not recognized")


def emit_typedef_encoder(environment: Environment, node: _XdrDeclaration) -> None:
    """Emit one encoder function for one typedef"""
    if isinstance(node, _XdrBasic):
        template = get_jinja2_template(environment, "encoder", node.template)
        print(
            template.render(
                name=node.name,
                type=node.spec.type_name,
            )
        )
    elif isinstance(node, _XdrVariableLengthString):
        template = get_jinja2_template(environment, "encoder", node.template)
        print(
            template.render(
                name=node.name,
                maxsize=node.maxsize,
            )
        )
    elif isinstance(node, _XdrFixedLengthOpaque):
        template = get_jinja2_template(environment, "encoder", node.template)
        print(
            template.render(
                name=node.name,
                size=node.size,
            )
        )
    elif isinstance(node, _XdrVariableLengthOpaque):
        template = get_jinja2_template(environment, "encoder", node.template)
        print(
            template.render(
                name=node.name,
                maxsize=node.maxsize,
            )
        )
    elif isinstance(node, _XdrFixedLengthArray):
        template = get_jinja2_template(environment, "encoder", node.template)
        print(
            template.render(
                name=node.name,
                type=node.spec.type_name,
                size=node.size,
            )
        )
    elif isinstance(node, _XdrVariableLengthArray):
        template = get_jinja2_template(environment, "encoder", node.template)
        print(
            template.render(
                name=node.name,
                type=node.spec.type_name,
                maxsize=node.maxsize,
            )
        )
    elif isinstance(node, _XdrOptionalData):
        raise NotImplementedError("<optional_data> typedef not yet implemented")
    elif isinstance(node, _XdrVoid):
        raise NotImplementedError("<void> typedef not yet implemented")
    else:
        raise NotImplementedError("typedef: type not recognized")


class XdrTypedefGenerator(SourceGenerator):
    """Generate source code for XDR typedefs"""

    def __init__(self, language: str, peer: str):
        """Initialize an instance of this class"""
        self.environment = create_jinja2_environment(language, "typedef")
        self.peer = peer

    def emit_declaration(self, node: _XdrTypedef) -> None:
        """Emit one declaration pair for an XDR enum type"""
        emit_typedef_declaration(self.environment, node.declaration)

    def emit_definition(self, node: _XdrTypedef) -> None:
        """Emit one definition for an XDR typedef"""
        emit_type_definition(self.environment, node.declaration)

    def emit_decoder(self, node: _XdrTypedef) -> None:
        """Emit one decoder function for an XDR typedef"""
        emit_typedef_decoder(self.environment, node.declaration)

    def emit_encoder(self, node: _XdrTypedef) -> None:
        """Emit one encoder function for an XDR typedef"""
        emit_typedef_encoder(self.environment, node.declaration)
