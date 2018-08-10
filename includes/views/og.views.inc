<?php


/**
 * @file
 * Provides support for the Views module.
 */

/**
 * Default controller for generating basic views integration.
 *
 * The controller tries to generate suiting views integration for the entity
 * based upon the schema information of its base table and the provided entity
 * property information.
 * For that it is possible to map a property name to its schema/views field
 * name by adding a 'schema field' key with the name of the field as value to
 * the property info.
 */
class EntityDefaultViewsController {

  protected $type, $info, $relationships;

  public function __construct($type) {
    $this->type = $type;
    $this->info = entity_get_info($type);
  }

  /**
   * Defines the result for hook_views_data().
   */
  public function views_data() {
    $data = array();
    $this->relationships = array();

    if (!empty($this->info['base table'])) {
      $table = $this->info['base table'];
      // Define the base group of this table. Fields that don't
      // have a group defined will go into this field by default.
      $data[$table]['table']['group']  = drupal_ucfirst($this->info['label']);
      $data[$table]['table']['entity type'] = $this->type;

      // If the plural label isn't available, use the regular label.
      $label = isset($this->info['plural label']) ? $this->info['plural label'] : $this->info['label'];
      $data[$table]['table']['base'] = array(
        'field' => $this->info['entity keys']['id'],
        'access query tag' => $this->type . '_access',
        'title' => drupal_ucfirst($label),
        'help' => isset($this->info['description']) ? $this->info['description'] : '',
      );
      $data[$table]['table']['entity type'] = $this->type;
      $data[$table] += $this->schema_fields();

      // Add in any reverse-relationships which have been determined.
      $data += $this->relationships;
    }
    if (!empty($this->info['revision table']) && !empty($this->info['entity keys']['revision'])) {
      $revision_table = $this->info['revision table'];

      $data[$table]['table']['default_relationship'] = array(
        $revision_table => array(
          'table' => $revision_table,
          'field' => $this->info['entity keys']['revision'],
        ),
      );

      // Define the base group of this table. Fields that don't
      // have a group defined will go into this field by default.
      $data[$revision_table]['table']['group']  = drupal_ucfirst($this->info['label']) . ' ' . t('Revisions');
      $data[$revision_table]['table']['entity type'] = $this->type;

      // If the plural label isn't available, use the regular label.
      $label = isset($this->info['plural label']) ? $this->info['plural label'] : $this->info['label'];
      $data[$revision_table]['table']['base'] = array(
        'field' => $this->info['entity keys']['revision'],
        'access query tag' => $this->type . '_access',
        'title' => drupal_ucfirst($label) . ' ' . t('Revisions'),
        'help' => (isset($this->info['description']) ? $this->info['description'] . ' ' : '') . t('Revisions'),
      );
      $data[$revision_table]['table']['entity type'] = $this->type;
      $data[$revision_table] += $this->schema_revision_fields();

      // Add in any reverse-relationships which have been determined.
      $data += $this->relationships;

      // For other base tables, explain how we join.
      $data[$revision_table]['table']['join'] = array(
        // Directly links to base table.
        $table => array(
          'left_field' => $this->info['entity keys']['revision'],
          'field' => $this->info['entity keys']['revision'],
        ),
      );
      $data[$revision_table]['table']['default_relationship'] = array(
        $table => array(
          'table' => $table,
          'field' => $this->info['entity keys']['id'],
        ),
      );
    }
    return $data;
  }

  /**
   * Try to come up with some views fields with the help of the schema and
   * the entity property information.
   */
  protected function schema_fields() {
    $schema = drupal_get_schema($this->info['base table']);
    $properties = entity_get_property_info($this->type) + array('properties' => array());
    $data = array();

    foreach ($properties['properties'] as $name => $property_info) {
      if (isset($property_info['schema field']) && isset($schema['fields'][$property_info['schema field']])) {
        if ($views_info = $this->map_from_schema_info($name, $schema['fields'][$property_info['schema field']], $property_info)) {
          $data[$name] = $views_info;
        }
      }
    }
    return $data;
  }

  /**
   * Try to come up with some views fields with the help of the revision schema
   * and the entity property information.
   */
  protected function schema_revision_fields() {
    $data = array();
    if (!empty($this->info['revision table'])) {
      $schema = drupal_get_schema($this->info['revision table']);
      $properties = entity_get_property_info($this->type) + array('properties' => array());

      foreach ($properties['properties'] as $name => $property_info) {
        if (isset($property_info['schema field']) && isset($schema['fields'][$property_info['schema field']])) {
          if ($views_info = $this->map_from_schema_info($name, $schema['fields'][$property_info['schema field']], $property_info)) {
            $data[$name] = $views_info;
          }
        }
      }
    }
    return $data;
  }

  /**
   * Comes up with views information based on the given schema and property
   * info.
   */
  protected function map_from_schema_info($property_name, $schema_field_info, $property_info) {
    $type = isset($property_info['type']) ? $property_info['type'] : 'text';
    $views_field_name = $property_info['schema field'];

    $return = array();

    if (!empty($schema_field_info['serialize'])) {
      return FALSE;
    }

    $description = array(
      'title' => $property_info['label'],
      'help' => isset($property_info['description']) ? $property_info['description'] : NULL,
    );

      // Add in relationships to related entities.
    if (($info = entity_get_info($type)) && !empty($info['base table'])) {

      // Prepare reversed relationship data.
      $label_lowercase = drupal_strtolower($this->info['label'][0]) . drupal_substr($this->info['label'], 1);
      $property_label_lowercase = drupal_strtolower($property_info['label'][0]) . drupal_substr($property_info['label'], 1);

      // We name the field of the first reverse-relationship just with the
      // base table to be backward compatible, for subsequents relationships we
      // append the views field name in order to get a unique name.
      $name = !isset($this->relationships[$info['base table']][$this->info['base table']]) ? $this->info['base table'] : $this->info['base table'] . '_' . $views_field_name;
      $this->relationships[$info['base table']][$name] = array(
        'title' => $this->info['label'],
        'help' => t("Associated @label via the @label's @property.", array('@label' => $label_lowercase, '@property' => $property_label_lowercase)),
        'relationship' => array(
          'label' => $this->info['label'],
          'handler' => $this->getRelationshipHandlerClass($this->type, $type),
          'base' => $this->info['base table'],
          'base field' => $views_field_name,
          'relationship field' => isset($info['entity keys']['name']) ? $info['entity keys']['name'] : $info['entity keys']['id'],
        ),
      );

      $return['relationship'] = array(
        'label' => drupal_ucfirst($info['label']),
        'handler' => $this->getRelationshipHandlerClass($type, $this->type),
        'base' => $info['base table'],
        'base field' => isset($info['entity keys']['name']) ? $info['entity keys']['name'] : $info['entity keys']['id'],
        'relationship field' => $views_field_name,
      );

      // Add in direct field/filters/sorts for the id itself too.
      $type = isset($info['entity keys']['name']) ? 'token' : 'integer';
      // Append the views-field-name to the title if it is different to the
      // property name.
      if ($property_name != $views_field_name) {
        $description['title'] .= ' ' . $views_field_name;
      }
    }

    switch ($type) {
      case 'token':
      case 'text':
        $return += $description + array(
          'field' => array(
            'real field' => $views_field_name,
            'handler' => 'views_handler_field',
            'click sortable' => TRUE,
           ),
          'sort' => array(
            'real field' => $views_field_name,
            'handler' => 'views_handler_sort',
          ),
          'filter' => array(
            'real field' => $views_field_name,
            'handler' => 'views_handler_filter_string',
          ),
          'argument' => array(
            'real field' => $views_field_name,
            'handler' => 'views_handler_argument_string',
          ),
        );
      break;

      case 'decimal':
      case 'integer':
        $return += $description + array(
          'field' => array(
            'real field' => $views_field_name,
            'handler' => 'views_handler_field_numeric',
            'click sortable' => TRUE,
            'float' => ($type == 'decimal'),
           ),
          'sort' => array(
            'real field' => $views_field_name,
            'handler' => 'views_handler_sort',
          ),
          'filter' => array(
            'real field' => $views_field_name,
            'handler' => 'views_handler_filter_numeric',
          ),
          'argument' => array(
            'real field' => $views_field_name,
            'handler' => 'views_handler_argument_numeric',
          ),
        );
      break;

      case 'date':
        $return += $description + array(
          'field' => array(
            'real field' => $views_field_name,
            'handler' => 'views_handler_field_date',
            'click sortable' => TRUE,
           ),
          'sort' => array(
            'real field' => $views_field_name,
            'handler' => 'views_handler_sort_date',
          ),
          'filter' => array(
            'real field' => $views_field_name,
            'handler' => 'views_handler_filter_date',
          ),
          'argument' => array(
            'real field' => $views_field_name,
            'handler' => 'views_handler_argument_date',
          ),
        );
      break;

      case 'duration':
        $return += $description + array(
          'field' => array(
            'real field' => $views_field_name,
            'handler' => 'entity_views_handler_field_duration',
            'click sortable' => TRUE,
           ),
          'sort' => array(
            'real field' => $views_field_name,
            'handler' => 'views_handler_sort',
          ),
          'filter' => array(
            'real field' => $views_field_name,
            'handler' => 'views_handler_filter_numeric',
          ),
          'argument' => array(
            'real field' => $views_field_name,
            'handler' => 'views_handler_argument_numeric',
          ),
        );
      break;

      case 'uri':
        $return += $description + array(
          'field' => array(
            'real field' => $views_field_name,
            'handler' => 'views_handler_field_url',
            'click sortable' => TRUE,
           ),
          'sort' => array(
            'real field' => $views_field_name,
            'handler' => 'views_handler_sort',
          ),
          'filter' => array(
            'real field' => $views_field_name,
            'handler' => 'views_handler_filter_string',
          ),
          'argument' => array(
            'real field' => $views_field_name,
            'handler' => 'views_handler_argument_string',
          ),
        );
      break;

      case 'boolean':
        $return += $description + array(
          'field' => array(
            'real field' => $views_field_name,
            'handler' => 'views_handler_field_boolean',
            'click sortable' => TRUE,
           ),
          'sort' => array(
            'real field' => $views_field_name,
            'handler' => 'views_handler_sort',
          ),
          'filter' => array(
            'real field' => $views_field_name,
            'handler' => 'views_handler_filter_boolean_operator',
          ),
          'argument' => array(
            'real field' => $views_field_name,
            'handler' => 'views_handler_argument_string',
          ),
        );
      break;
    }

    // If there is an options list callback, add to the filter and field.
    if (isset($return['filter']) && !empty($property_info['options list'])) {
      $return['filter']['handler'] = 'views_handler_filter_in_operator';
      $return['filter']['options callback'] = array('EntityDefaultViewsController', 'optionsListCallback');
      $return['filter']['options arguments'] = array($this->type, $property_name, 'view');
    }
    // @todo: This class_exists is needed until views 3.2.
    if (isset($return['field']) && !empty($property_info['options list']) && class_exists('views_handler_field_machine_name')) {
      $return['field']['handler'] = 'views_handler_field_machine_name';
      $return['field']['options callback'] = array('EntityDefaultViewsController', 'optionsListCallback');
      $return['field']['options arguments'] = array($this->type, $property_name, 'view');
    }
    return $return;
  }

  /**
   * Determines the handler to use for a relationship to an entity type.
   *
   * @param $entity_type
   *   The entity type to join to.
   * @param $left_type
   *   The data type from which to join.
   */
  function getRelationshipHandlerClass($entity_type, $left_type) {
    // Look for an entity type which is used as bundle for the given entity
    // type. If there is one, allow filtering the relation by bundle by using
    // our own handler.
    foreach (entity_get_info() as $type => $info) {
      // In case we already join from the bundle entity we do not need to filter
      // by bundle entity any more, so we stay with the general handler.
      if (!empty($info['bundle of']) && $info['bundle of'] == $entity_type && $type != $left_type) {
        return 'entity_views_handler_relationship_by_bundle';
      }
    }
    return 'views_handler_relationship';
  }

  /**
   * A callback returning property options, suitable to be used as views options callback.
   */
  public static function optionsListCallback($type, $selector, $op = 'view') {
    $wrapper = entity_metadata_wrapper($type, NULL);
    $parts = explode(':', $selector);
    foreach ($parts as $part) {
      $wrapper = $wrapper->get($part);
    }
    return $wrapper->optionsList($op);
  }
}



/**
 * "OG membership" entity Views definitions.
 */
class OgMembershipViewsController extends EntityDefaultViewsController {

  /**
   * Override views_data().
   */
  public function views_data() {
    $data = parent::views_data();

    $data['og_membership']['state']['filter']['handler'] = 'og_handler_filter_group_audience_state';
    $data['og_membership']['state']['field']['handler'] = 'og_handler_field_group_audience_state';

    $data['og_membership']['etid'] = array(
      'title' => t('Entity id'),
      'help' => t('Og membership "etid" property.'),
      'field' => array(
        'handler' => 'views_handler_field_numeric',
        'click sortable' => TRUE,
      ),
      'sort' => array(
        'handler' => 'views_handler_sort',
      ),
      'filter' => array(
        'handler' => 'views_handler_filter_numeric',
      ),
      'argument' => array(
        'handler' => 'views_handler_argument_numeric',
      ),
    );

    // Improve help text.
    $data['og_membership']['gid'] = $data['og_membership']['etid'];
    $data['og_membership']['gid']['title'] = t('Group ID');
    $data['og_membership']['gid']['help'] = t('Og membership "gid" property.');

    $data['og_membership']['og_roles'] = array(
      'title' => t('OG user roles in group'),
      'help' => t('Show all the roles a user belongs to in a group. Requires a relationship to users to be present.'),
      // This is a dummy field, so point it to a real field that we need - the
      // group ID
      'real field' => 'gid',
      'field' => array(
        'handler' => 'og_handler_field_user_roles',
      ),
    );

    $data['og_membership']['og_users_roles'] = array(
      'title' => t('OG Roles from membership'),
      'help' => t('The OG Roles associated with the OG membership'),
      // Provide a possible relationship to roles through uid.
      'relationship' => array(
        'label' => t('OG Roles from OG membership'),
        'handler' => 'og_handler_relationship_membership_roles',
        'base' => 'og_users_roles',
        'base field' => 'uid',
        'relationship field' => 'etid',
      ),
    );

    $data['og_role'] = array(
      'table' => array(
        'group' => t('OG user roles'),
        'join' => array(
          // Attach automatically to og_users_roles relations.
          'og_users_roles' => array(
            'left_field' => 'rid',
            'field' => 'rid',
          ),
        ),
      ),
    );
    $data['og_role']['name'] = array(
      'title' => t('Role Name'),
      'help' => t('The OG role name.'),
      'filter' => array(
        'handler' => 'og_handler_filter_user_roles',
      ),
    );

    // Link to edit membership.
    $data['og_membership']['edit_membership'] = array(
      'field' => array(
        'title' => t('Edit link'),
        'help' => t('Provide a simple link to edit the membership.'),
        'handler' => 'og_handler_field_og_membership_link_edit',
      ),
    );

    // Link to delete membership.
    $data['og_membership']['delete_membership'] = array(
      'field' => array(
        'title' => t('Delete link'),
        'help' => t('Provide a simple link to delete the membership.'),
        'handler' => 'og_handler_field_og_membership_link_delete',
      ),
    );

    return $data;
  }
}

/**
 * Implements hook_views_data_alter().
 */
function og_views_data_alter(&$data) {
  $group_content_entities = og_get_all_group_content_entity();
  $group_entity_types = og_get_all_group_entity();

  foreach (entity_get_info() as $entity_type => $info) {
    if (empty($group_content_entities[$entity_type]) && empty($group_entity_types[$entity_type])) {
      continue;
    }

    // OG membership relationship.
    $data[$info['base table']]['og_membership_rel'] = array(
      'group' => t('OG membership'),
      'title' => t('OG membership from @entity', array('@entity' => ucfirst($info['label']))),
      'help' => t('The OG membership associated with the @entity entity.', array('@entity' => ucfirst($info['label']))),
      'relationship' => array(
        // Pass the entity to the handler.
        'entity' => $entity_type,
        'handler' => 'og_handler_relationship',
        'label' => t('OG membership from @entity', array('@entity' => $entity_type)),
        'base' => 'og_membership',
        'base field' => 'etid',
        'relationship field' => $info['entity keys']['id'],
      ),
    );

    // The OG membership group.
    $data[$info['base table']]['og_membership_rel_group'] = array(
      'group' => t('OG membership'),
      'title' => t('OG membership from @entity group', array('@entity' => ucfirst($info['label']))),
      'help' => t('The OG membership associated with the @entity group', array('@entity' => ucfirst($info['label']))),
      'relationship' => array(
        'group_type' => $entity_type,
        'handler' => 'og_handler_relationship',
        'label' => t('OG membership from @entity group', array('@entity' => $entity_type)),
        'base' => 'og_membership',
        'base field' => 'gid',
        'relationship field' => $info['entity keys']['id'],
      ),
    );

    // OG membership's related entity.
    $data['og_membership']['og_membership_related_' . $entity_type] = array(
      'group' => t('OG membership'),
      'title' => t('@entity from OG membership', array('@entity' => ucfirst($info['label']))),
      'help' => t('The @entity entity that is associated with the OG membership.', array('@entity' => $info['label'])),
      'relationship' => array(
        'entity' => $entity_type,
        'handler' => 'og_handler_relationship',
        'label' => t('@entity from OG membership', array('@entity' => $entity_type)),
        'base' => $info['base table'],
        'base field' => $info['entity keys']['id'],
        'relationship field' => 'etid',
      ),
    );

    // OG membership's related group.
    $data['og_membership']['og_membership_related_' . $entity_type . '_group'] = array(
      'group' => t('OG membership'),
      'title' => t('Group @entity from OG membership', array('@entity' => ucfirst($info['label']))),
      'help' => t('The @entity group that is associated with the OG membership.', array('@entity' => $info['label'])),
      'relationship' => array(
        'group_type' => $entity_type,
        'handler' => 'og_handler_relationship',
        'label' => t('Group @entity from OG membership', array('@entity' => $entity_type)),
        'base' => $info['base table'],
        'base field' => $info['entity keys']['id'],
        'relationship field' => 'gid',
      ),
    );
  }

  $data['field_data_group_audience']['user_roles'] = array(
    'group' => t('Group'),
    'title' => t('Group user roles'),
    'help' => t('Show all the roles a user belongs to in a group.'),
    // This is a dummy field, so point it to a real field that we need - the
    // group ID
    'real field' => 'group_audience_gid',
    'field' => array(
      'handler' => 'og_handler_field_user_roles',
    ),
  );

  $data['field_data_group_audience']['og_permissions'] = array(
    'group' => t('Group'),
    'title' => t('Group permissions'),
    'help' => t('Filter by group permissions.'),
    // This is a dummy field, so point it to a real field that we need - the
    // group ID
    'real field' => 'group_audience_gid',
    'field' => array(
      'handler' => 'og_handler_field_group_permissions',
    ),
  );
}

/**
 * Implements hook_views_plugins().
 */
function og_views_plugins() {
  return array(
    'argument validator' => array(
      'og' => array(
        'title' => t('OG group'),
        'handler' => 'og_plugin_argument_validate_group',
        'path' => backdrop_get_path('module', 'og') . '/includes/views/handlers',
      ),
    ),
    'argument default' => array(
      'og_user_groups' => array(
        'title' => t('The OG groups of the logged in user'),
        'handler' => 'og_plugin_argument_default_user_groups',
      ),
    ),
  );
}
