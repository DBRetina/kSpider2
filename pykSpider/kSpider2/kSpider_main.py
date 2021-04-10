import sys
import click

from kSpider2.click_context import cli
from kSpider2.ks_pairwise import main as pairwise_main   # pylint: disable=relative-beyond-top-level
from kSpider2.ks_indexing import main as indexing_main   # pylint: disable=relative-beyond-top-level


cli.add_command(pairwise_main, name="pairwise")
cli.add_command(indexing_main, name="items_indexing")


if __name__ == '__main__':
    cli()
