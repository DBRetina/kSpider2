#!/usr/bin/python
# -*- coding: utf-8 -*-
from __future__ import division

import sys

import _kSpider_internal as kSpider_internal

# try:
#     import pykSpider.kSpider_internal
# except ImportError:
#     print("kSpider_internal is not built yet", file = sys.stderr)

import click
from kSpider2.click_context import cli


@cli.command(name = "items_indexing", help_priority=1)
@click.option('-i', '--items-file', required=True, type=click.STRING, help="Items file")
@click.option('-n', '--names-file', required=True, type=click.STRING, help="Names file")
@click.option('-p', '--index-prefix', required=True, type=click.STRING, help="Index prefix")
@click.pass_context
def main(ctx, items_file, names_file, index_prefix):
    """
    Indexing items using listDecoder and kProcessor.
    """

    kSpider_internal.items_indexing(items_file, names_file, index_prefix)
