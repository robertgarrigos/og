<?php

/**
 * @file
 * A class used for group membership types.
 */
class OgMembershipType extends Entity {

  public $name;
  public $description = '';

  /**
   * @todo Please document this function.
   * @see http://drupal.org/node/1354
   */
  public function __construct($values = array()) {
    parent::__construct($values, 'og_membership_type');
  }
}
